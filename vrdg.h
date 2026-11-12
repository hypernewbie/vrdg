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
    VRDG_DISCARD = 0x10,
    VRDG_COLOR = 0x20,
    VRDG_DEPTH = 0x40,
    VRDG_SAMPLED = 0x80,
    VRDG_STORAGE = 0x100,
    VRDG_COPY = 0x200,
    VRDG_INDIRECT = 0x400,
    VRDG_USAGE_MASK = VRDG_COLOR | VRDG_DEPTH | VRDG_SAMPLED | VRDG_STORAGE | VRDG_COPY | VRDG_INDIRECT
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
    const char* source = "";
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
    bool descriptorsResolved = false;
};

inline std::string DescribeFlags( uint64_t flags )
{
    const struct { uint64_t bit; const char* name; } names[] = {
        { VRDG_READ, "read" }, { VRDG_WRITE, "write" },
        { VRDG_GRAPHICS, "graphics" }, { VRDG_COMPUTE, "compute" }, { VRDG_DISCARD, "discard" },
        { VRDG_COLOR, "color" }, { VRDG_DEPTH, "depth" }, { VRDG_SAMPLED, "sampled" },
        { VRDG_STORAGE, "storage" }, { VRDG_COPY, "copy" }, { VRDG_INDIRECT, "indirect" }
    };
    std::string text;
    for( const auto& entry : names ) {
        if ( flags & entry.bit ) {
            if ( !text.empty() ) text += "|";
            text += entry.name;
            flags &= ~entry.bit;
        }
    }
    if ( flags ) text += ( text.empty() ? "" : "|" ) + std::string( "unknown=" ) + std::to_string( flags );
    return text.empty() ? "none" : text;
}

inline const char* AccessError( AttachmentType type, uint64_t flags, uint32_t taskFlags )
{
    uint64_t roles = flags & VRDG_USAGE_MASK;
    if ( !( flags & ( VRDG_READ | VRDG_WRITE ) ) )
        return "access requires read or write";
    if ( ( flags & VRDG_DISCARD ) && ( !( flags & VRDG_WRITE ) || ( flags & VRDG_READ ) ) )
        return "discard access cannot read existing contents";
    if ( type == VRDG_TYPE_BUFFER && ( roles & ( VRDG_COLOR | VRDG_DEPTH | VRDG_SAMPLED ) ) )
        return "buffer access cannot use texture roles";
    if ( type == VRDG_TYPE_FB && ( roles & VRDG_INDIRECT ) )
        return "indirect access requires a buffer";
    if ( ( roles & ( VRDG_COLOR | VRDG_DEPTH ) ) && !( taskFlags & VRDG_GRAPHICS ) )
        return "attachment access requires a graphics task";
    if ( ( roles & ( VRDG_SAMPLED | VRDG_INDIRECT ) ) && ( flags & VRDG_WRITE ) )
        return "sampled and indirect access must be read-only";
    if ( ( roles & VRDG_COLOR ) && !( flags & VRDG_WRITE ) )
        return "color attachment access must write";
    if ( ( roles & VRDG_COPY ) && ( flags & VRDG_READ ) && ( flags & VRDG_WRITE ) )
        return "copy access must be read-only or write-only";
    if ( ( flags & VRDG_WRITE ) && ( roles & ( roles - 1 ) ) )
        return "conflicting access roles";
    return nullptr;
}

inline std::string Quote( const std::string& value )
{
    std::string text = "\"";
    for( unsigned char c : value ) {
        if ( c == '"' || c == '\\' ) { text += '\\'; text += c; }
        else if ( c == '\n' ) text += "\\n";
        else if ( c == '\r' ) text += "\\r";
        else if ( c == '\t' ) text += "\\t";
        else if ( c < 32 || c == 127 ) {
            const char* hex = "0123456789abcdef";
            text += "\\x"; text += hex[c >> 4]; text += hex[c & 15];
        }
        else text += c;
    }
    return text + "\"";
}

inline std::string Describe( const State& state )
{
    std::string text = std::string( "graph compiled=" ) + ( state.compiled ? "true" : "false" ) +
        " descriptors=" + ( state.descriptorsResolved ? "resolved" : "unresolved" ) +
        " tasks=" + std::to_string( state.tasks.size() ) + " resources=" + std::to_string( state.resources.size() ) + "\n";
    for( size_t i = 0; i < state.resources.size(); i++ ) {
        const auto& r = state.resources[i];
        const char* type = r.type == VRDG_TYPE_BUFFER ? "buffer" : r.type == VRDG_TYPE_FB ? "texture" : "unknown";
        const char* lifetime = r.lifetime == VRDG_TRANSIENT ? "transient" : r.lifetime == VRDG_PERSISTENT ? "persistent" : r.lifetime == VRDG_IMPORTED ? "imported" : "unknown";
        text += "resource " + std::to_string( i ) + " " + Quote( r.name ) + " " + type + " " + lifetime +
            " declared=" + std::to_string( r.declarationTask ) + " first=" + std::to_string( r.firstUse ) + " last=" + std::to_string( r.lastUse );
        if ( !state.descriptorsResolved ) text += " descriptor=unresolved";
        else if ( r.type == VRDG_TYPE_BUFFER )
            text += " size=" + std::to_string( r.buffer.size ) + " format=" + std::to_string( r.buffer.format ) + " flags=" + std::to_string( r.buffer.flags );
        else if ( r.type == VRDG_TYPE_FB )
            text += " width=" + std::to_string( r.fb.width ) + " height=" + std::to_string( r.fb.height ) + " format=" + std::to_string( r.fb.format ) +
                " mips=" + std::to_string( r.fb.mips ) + " flags=" + std::to_string( r.fb.flags );
        text += "\n";
    }
    for( size_t i = 0; i < state.tasks.size(); i++ ) {
        const auto& task = state.tasks[i];
        text += "task " + std::to_string( i ) + " " + Quote( task.name ) + " " + DescribeFlags( task.flags ) + " source=" + Quote( task.source ? task.source : "" ) + "\n";
        for( size_t j = 0; j < task.uses.size(); j++ ) {
            const auto& use = task.uses[j];
            const char* type = use.type == VRDG_TYPE_BUFFER ? "buffer" : use.type == VRDG_TYPE_FB ? "texture" : "unknown";
            text += "  arg " + std::to_string( j ) + " resource=" + std::to_string( use.id ) + " type=" + type + " access=" + DescribeFlags( use.flags );
            if ( !use.flags ) {
                for( size_t k = 0; k < j; k++ ) {
                    if ( task.uses[k].id == use.id && task.uses[k].flags ) {
                        text += " merged_into=" + std::to_string( k );
                        break;
                    }
                }
            }
            if ( use.id >= state.resources.size() ) text += " undeclared";
            text += "\n";
        }
    }
    return text;
}

inline std::logic_error ResourceError( const State* state, int taskIndex, uint64_t id, const std::string& message )
{
    std::string text = "VRDG: ";
    if ( taskIndex >= 0 && taskIndex < ( int ) state->tasks.size() ) {
        const auto& task = state->tasks[taskIndex];
        if ( task.source && task.source[0] )
            text += std::string( task.source ) + ": ";
        text += "task " + std::to_string( taskIndex ) + " " + Quote( task.name ) + ", ";
    }
    text += "resource " + std::to_string( id );
    if ( id < state->resources.size() )
        text += " " + Quote( state->resources[id].name );
    return std::logic_error( text + ": " + message );
}

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

    // Establish the alias occupant, then supply required initial contents before the first transition.
    virtual void ActivateResource( State* state, const Resource& resource, uint64_t flags )
    {
        if ( resource.lifetime == VRDG_TRANSIENT && !( flags & VRDG_DISCARD ) )
            throw ResourceError( state, resource.firstUse, resource.id, "first access needs initial contents; use a discard write or initialize during activation" );
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
    state->descriptorsResolved = false;
    state->tasks.clear();
    state->resources.clear();
    ei->Build( state );

    for( int i = 0; i < ( int ) state->tasks.size(); i++ ) {
        auto& task = state->tasks[i];
        for( int j = 0; j < ( int ) task.uses.size(); j++ ) {
            auto& use = task.uses[j];
            if ( use.id >= state->resources.size() || state->resources[use.id].type != use.type )
                throw ResourceError( state, i, use.id, "access requires a matching resource declaration" );
            if ( const char* error = AccessError( use.type, use.flags, task.flags ) )
                throw ResourceError( state, i, use.id, std::string( error ) + " (" + DescribeFlags( use.flags ) + ")" );

            // Keep callback argument slots. Repeated IDs transition through their first slot.
            for( int k = 0; k < j; k++ ) {
                auto& previous = task.uses[k];
                if ( previous.id != use.id || !previous.flags )
                    continue;
                if ( ( previous.flags & VRDG_WRITE ) && ( use.flags & VRDG_WRITE ) && ( ( previous.flags ^ use.flags ) & VRDG_DISCARD ) )
                    throw ResourceError( state, i, use.id, "conflicting discard and preserving writes" );
                previous.flags |= use.flags;
                use.flags = 0;
                if ( const char* error = AccessError( previous.type, previous.flags, task.flags ) )
                    throw ResourceError( state, i, use.id, std::string( error ) + " (" + DescribeFlags( previous.flags ) + ")" );
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
    state->descriptorsResolved = false;
    for( auto& resource : state->resources )
        resource.resolveDescriptor( state, ei, &state->tasks[resource.declarationTask], &resource );
    state->descriptorsResolved = true;
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
