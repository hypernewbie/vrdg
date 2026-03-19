/*
    -- VRDG --

    Copyright 2026 UAA Software

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
    associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial
    portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
    OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

#ifndef VRDG_BUFFER
    #define VRDG_BUFFER uint64_t
#endif
#ifndef VRDG_FRAMEBUFFER
    #define VRDG_FRAMEBUFFER uint64_t
#endif

#ifndef MICROPROFILE_SCOPEI
    #define MICROPROFILE_SCOPEI(group, name, color) ((void)0)
#endif
#ifndef MICROPROFILE_SCOPEGPUI
    #define MICROPROFILE_SCOPEGPUI(name, color) ((void)0)
#endif

#ifndef VRHI_H
    typedef uint64_t vhTimerID;
    inline void vhBeginTimerQuery(vhTimerID id) {}
    inline void vhEndTimerQuery(vhTimerID id) {}
#endif

namespace vrdg {

typedef uint64_t Buffer;
typedef uint64_t FrameBuffer;

struct Task;
struct State;
class EngineInterface;

enum TransitionFlagBits
{
    VRDG_READ = 0x1,
    VRDG_WRITE = 0x2,
    VRDG_GRAPHICS = 0x4,
    VRDG_COMPUTE = 0x8
};

enum AttachmentType
{
    VRDG_TYPE_BUFFER,
    VRDG_TYPE_FB
};

struct Task
{
    std::string name;
    uint32_t flags = 0;

    int createOutputIdx = 0;
    std::vector< uint64_t > input;
    std::vector< uint64_t > output;
    std::vector< uint64_t > releases;
    std::vector< AttachmentType > inputType;
    std::vector< AttachmentType > outputType;
    std::vector< AttachmentType > releaseType;

    std::function< void( State*, EngineInterface*, Task* ) > callback = nullptr;
};

struct State
{
    uint64_t currentBuildPlaceholderID = 0;
    uint64_t currentGraphHash0 = 0;
    uint64_t currentGraphHash1 = 0;
    std::vector< Task > tasks;
};

class EngineInterface
{
    std::vector< uint32_t > m_options;

public:
    virtual ~EngineInterface() {}
    
    virtual void CreateBuffer( Buffer idx, const std::string& name, uint64_t sz, uint32_t format, uint32_t flags )
    {
    }

    virtual void ReleaseBuffer( Buffer idx )
    {
    }

    virtual VRDG_BUFFER GetEngineBufferObject( Buffer idx )
    {
        return VRDG_BUFFER();
    }

    virtual void TransitionBuffer( Buffer idx, uint64_t flags )
    {
    }

    virtual void CreateFB( FrameBuffer idx, const std::string& name, uint32_t width, uint32_t height, uint32_t format, uint32_t mips, uint64_t flags = 0 )
    {
    }

    virtual void ReleaseFB( FrameBuffer idx )
    {
    }

    virtual VRDG_FRAMEBUFFER GetEngineFBObject( FrameBuffer idx )
    {
        return VRDG_FRAMEBUFFER();
    }

    virtual void TransitionFB( FrameBuffer idx, uint64_t flags )
    {
    }

    virtual void HashGraphConfigOptions( uint64_t& hash0, uint64_t& hash1 )
    {
        // JS Hash Function src: https://www.partow.net/programming/hashfunctions/index.html#RSHashFunction
        hash0 = 1315423911; hash1 = 13;
        for( int i = 0; i < m_options.size(); i++ )
        {
            hash0 ^= ( ( hash0 << 5 ) + ( m_options[i] ) + ( hash0 >> 2 ) );
            hash1 ^= ( ( hash1 << 5 ) + ( m_options[i] ) + ( hash1 >> 2 ) );
        }
    }

    virtual void Build( State* s )
    {
    }
};

inline void Compile( State* state, EngineInterface* ei )
{
    // If we're already at latest, don't both rebuilding.
    uint64_t graphHash0 = 0, graphHash1 = 0;
    ei->HashGraphConfigOptions( graphHash0, graphHash1 );
    if ( graphHash0 == state->currentGraphHash0 && graphHash1 == state->currentGraphHash1 )
        return;

    // Clear build task list.
    state->currentBuildPlaceholderID = 0;
    state->tasks.clear();

    // Call overridden build step.
    ei->Build( state );

    // Store the hash so we can detect changes next time.
    state->currentGraphHash0 = graphHash0;
    state->currentGraphHash1 = graphHash1;

    // Post-build loop to determine last used resource times to release them.

    std::map< uint64_t, AttachmentType > attachmentType;
    std::map< uint64_t, int > lastUsedTask;

    for( int i = 0; i < state->tasks.size(); i++ ) {
        auto& task = state->tasks[i];
        for ( int j = 0; j < ( int ) task.input.size(); j++ ) {
            attachmentType[task.input[j]] = task.inputType[j];
            lastUsedTask[task.input[j]] = i;
        }
        for ( int j = 0; j < ( int ) task.output.size(); j++ ) {
            attachmentType[task.output[j]] = task.outputType[j];
            lastUsedTask[task.output[j]] = i;
        }
    }

    for ( auto& it : attachmentType )
    {
        int lastUsed = lastUsedTask[it.first];
        auto& task = state->tasks[lastUsed];
        task.releases.push_back( it.first );
        task.releaseType.push_back( it.second );
    }
}

inline void Run( State* state, EngineInterface* ei )
{
    for( auto& task : state->tasks ) {

        // Transition all inputs and outputs.

        for ( int i = 0; i < ( int ) task.input.size(); i++ ) {
            if ( task.inputType[i] == VRDG_TYPE_BUFFER )
                ei->TransitionBuffer( task.input[i], task.flags | VRDG_READ );
            else if ( task.inputType[i] == VRDG_TYPE_FB )
                ei->TransitionFB( task.input[i], task.flags | VRDG_READ );
        }

        for ( int i = 0; i < ( int ) task.output.size(); i++ ) {
            if ( task.outputType[i] == VRDG_TYPE_BUFFER )
                ei->TransitionBuffer( task.output[i], task.flags | VRDG_WRITE );
            else if ( task.outputType[i] == VRDG_TYPE_FB )
                ei->TransitionFB( task.output[i], task.flags | VRDG_WRITE );
        }
        
        // printf("Running task %d: %s\n", i, task.name.c_str());
        task.callback( state, ei, &task );

        for ( int i = 0; i < ( int ) task.releases.size(); i++ ) {
            if ( task.releaseType[i] == VRDG_TYPE_BUFFER )
                ei->ReleaseBuffer( task.releases[i] );
            else if ( task.releaseType[i] == VRDG_TYPE_FB )
                ei->ReleaseFB( task.releases[i] );
        }
    }
}

} // namespace vrdg 