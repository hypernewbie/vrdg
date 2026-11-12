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
#include <functional>
#include <cstdint>
#include <stdexcept>

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

#ifndef VRDG_GPU_PROFILE_BEGIN
    #define VRDG_GPU_PROFILE_BEGIN(id) ((void)0)
#endif
#ifndef VRDG_GPU_PROFILE_END
    #define VRDG_GPU_PROFILE_END(id) ((void)0)
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
    VRDG_COMPUTE = 0x8,
    VRDG_DISCARD = 0x10
};

enum AttachmentType
{
    VRDG_TYPE_BUFFER,
    VRDG_TYPE_FB
};

enum ResourceLifetime
{
    VRDG_TRANSIENT,
    VRDG_PERSISTENT,
    VRDG_IMPORTED
};

struct BufferDesc
{
    uint64_t size;
    uint32_t format, flags;

    BufferDesc( uint64_t sz = 0, uint32_t fmt = 0, uint32_t f = 0 ) : size( sz ), format( fmt ), flags( f ) {}

    bool operator==( const BufferDesc& other ) const
    {
        return size == other.size && format == other.format && flags == other.flags;
    }
};

struct FBDesc
{
    uint32_t width, height, format, mips;
    uint64_t flags;

    FBDesc( uint32_t w = 0, uint32_t h = 0, uint32_t fmt = 0, uint32_t m = 1, uint64_t f = 0 )
        : width( w ), height( h ), format( fmt ), mips( m ), flags( f ) {}

    bool operator==( const FBDesc& other ) const
    {
        return width == other.width && height == other.height && format == other.format && mips == other.mips && flags == other.flags;
    }
};

struct ResourceUse
{
    uint64_t id;
    AttachmentType type;
    uint64_t flags;
};

struct Resource
{
    uint64_t id = 0;
    std::string name;
    AttachmentType type = VRDG_TYPE_BUFFER;
    ResourceLifetime lifetime = VRDG_TRANSIENT;
    int declarationTask = -1;
    int firstUse = -1, lastUse = -1;
    BufferDesc buffer;
    FBDesc fb;

    std::function< void( State*, EngineInterface*, Task*, Resource* ) > resolveDescriptor = nullptr;
};

struct Task
{
    std::string name;
    uint32_t flags = 0;

    std::vector< ResourceUse > uses;

    std::function< void( State*, EngineInterface*, Task* ) > callback = nullptr;
};

struct State
{
    uint64_t currentGraphHash0 = 0;
    uint64_t currentGraphHash1 = 0;
    std::vector< Task > tasks;
    std::vector< Resource > resources;
    bool compiled = false;
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

    // All descriptors are current. Override to plan allocations and ensure/bind persistent and imported objects.
    virtual void PrepareResources( State* state )
    {
        for( auto& resource : state->resources ) {
            if ( resource.firstUse < 0 || resource.lifetime != VRDG_TRANSIENT )
                continue;
            if ( resource.type == VRDG_TYPE_BUFFER )
                CreateBuffer( resource.id, resource.name, resource.buffer.size, resource.buffer.format, resource.buffer.flags );
            else
                CreateFB( resource.id, resource.name, resource.fb.width, resource.fb.height, resource.fb.format, resource.fb.mips, resource.fb.flags );
        }
    }

    // Before first transition each execution. The engine tracks physical alias occupants.
    virtual void ActivateResource( State* state, const Resource& resource, uint64_t flags )
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
    uint64_t graphHash0 = 0, graphHash1 = 0;
    ei->HashGraphConfigOptions( graphHash0, graphHash1 );
    if ( state->compiled && graphHash0 == state->currentGraphHash0 && graphHash1 == state->currentGraphHash1 )
        return;

    state->compiled = false;
    state->tasks.clear();
    state->resources.clear();
    ei->Build( state );

    for( int i = 0; i < ( int ) state->tasks.size(); i++ ) {
        auto& task = state->tasks[i];
        for( int j = 0; j < ( int ) task.uses.size(); j++ ) {
            auto& use = task.uses[j];
            if ( use.id >= state->resources.size() || state->resources[use.id].type != use.type )
                throw std::logic_error( "VRDG: access requires a matching resource declaration" );

            // Keep callback argument slots. Repeated IDs transition through their first slot.
            for( int k = 0; k < j; k++ ) {
                auto& previous = task.uses[k];
                if ( previous.id != use.id || !previous.flags )
                    continue;
                if ( ( previous.flags & VRDG_WRITE ) && ( use.flags & VRDG_WRITE ) && ( ( previous.flags ^ use.flags ) & VRDG_DISCARD ) )
                    throw std::logic_error( "VRDG: conflicting discard and preserving writes" );
                previous.flags |= use.flags;
                use.flags = 0;
                if ( ( previous.flags & VRDG_DISCARD ) && ( previous.flags & VRDG_READ ) )
                    throw std::logic_error( "VRDG: discard access cannot read existing contents" );
                break;
            }
            auto& resource = state->resources[use.id];
            if ( resource.firstUse < 0 )
                resource.firstUse = i;
            resource.lastUse = i;
        }
    }

    state->currentGraphHash0 = graphHash0;
    state->currentGraphHash1 = graphHash1;
    state->compiled = true;
}

inline void Run( State* state, EngineInterface* ei )
{
    if ( !state->compiled )
        throw std::logic_error( "VRDG: Compile required before execution" );
    for( auto& resource : state->resources )
        resource.resolveDescriptor( state, ei, &state->tasks[resource.declarationTask], &resource );
    ei->PrepareResources( state );

    for( int i = 0; i < ( int ) state->tasks.size(); i++ ) {
        auto& task = state->tasks[i];
        // Activate all new occupants before any transitions for this task.
        for( auto& use : task.uses ) {
            auto& resource = state->resources[use.id];
            if ( use.flags && resource.firstUse == i )
                ei->ActivateResource( state, resource, task.flags | use.flags );
        }
        for( auto& use : task.uses ) {
            if ( !use.flags )
                continue;
            if ( use.type == VRDG_TYPE_BUFFER )
                ei->TransitionBuffer( use.id, task.flags | use.flags );
            else
                ei->TransitionFB( use.id, task.flags | use.flags );
        }
        task.callback( state, ei, &task );
        for( auto& use : task.uses ) {
            auto& resource = state->resources[use.id];
            if ( !use.flags || resource.lastUse != i || resource.lifetime != VRDG_TRANSIENT )
                continue;
            if ( use.type == VRDG_TYPE_BUFFER )
                ei->ReleaseBuffer( use.id );
            else
                ei->ReleaseFB( use.id );
        }
    }
}

} // namespace vrdg
