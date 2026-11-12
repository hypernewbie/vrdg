#include <cstdio>
#include <cstdlib>
#include "vrdg.h"
#include "test_access.h"

///> Graphics_Task( DeclareAccess )
///>     Create_TextureFB( color, 64, 32, 1, 1 )
///>     Create_TextureFB( depth, 64, 32, 2, 1 )
///>     Create_TextureFB( history, 64, 32, 1, 1 )
///>     Lifetime( history, Persistent )
///>     Create_Buffer( commands, 64, 0, 0 )
///>     Create_Buffer( destination, 64, 0, 0 )
///> End_Task

///> Graphics_Task( ProduceAccess )
///>     Write_TextureFB( color, Color, Discard )
///>     Write_TextureFB( depth, Depth, Discard )
///>     Write_Buffer( commands, Storage, Discard )
///> End_Task

///> Compute_Task( ProcessAccess )
///>     Read_TextureFB( color, Sampled )
///>     Read_TextureFB( depth, Sampled )
///>     ReadWrite_TextureFB( history, Storage )
///>     Read_Buffer( commands, Copy )
///>     Write_Buffer( destination, Copy, Discard )
///> End_Task

///> Graphics_Task( ConsumeAccess )
///>     Read_Buffer( shader, Storage )
///>     Read_Buffer( indirect, Indirect )
///>     Read_Buffer( shader, Indirect )
///>     Read_TextureFB( depth, Depth )
///>     Func( ConsumeAccess_Func )
///> End_Task

///> Graphics_Task( ConflictingRoles )
///>     Write_TextureFB( attachment, Color, Discard )
///>     Write_TextureFB( storage, Storage, Discard )
///> End_Task

void Check( bool condition, const char* message )
{
    if ( !condition ) {
        std::fprintf( stderr, "[FAIL] %s\n", message );
        std::exit( 1 );
    }
}

class AccessEngine : public vrdg::EngineInterface
{
public:
    bool conflicting = false;
    int callbacks = 0;
    std::vector< vrdg::ResourceUse > transitions;

    void Build( vrdg::State* s ) override
    {
        vrdg::FrameBuffer color, depth, history;
        vrdg::Buffer commands, destination;
        vrdg::DeclareAccess( s, color, depth, history, commands, destination );
        if ( conflicting ) {
            vrdg::ConflictingRoles( s, color, color );
            return;
        }
        vrdg::ProduceAccess( s, color, depth, commands );
        vrdg::ProcessAccess( s, color, depth, history, commands, destination );
        vrdg::ConsumeAccess( s, commands, commands, depth );
    }

    void TransitionBuffer( vrdg::Buffer id, uint64_t flags ) override { transitions.push_back( { id, vrdg::VRDG_TYPE_BUFFER, flags } ); }
    void TransitionFB( vrdg::FrameBuffer id, uint64_t flags ) override { transitions.push_back( { id, vrdg::VRDG_TYPE_FB, flags } ); }
    VRDG_BUFFER GetEngineBufferObject( vrdg::Buffer id ) override { return 100 + id; }
    VRDG_FRAMEBUFFER GetEngineFBObject( vrdg::FrameBuffer id ) override { return 200 + id; }
};

static AccessEngine* engine = nullptr;

namespace vrdg {
void ConsumeAccess_Func( VRDG_BUFFER shader, VRDG_BUFFER indirect, VRDG_FRAMEBUFFER depth )
{
    Check( shader == 103 && indirect == shader && depth == 201, "merged roles preserve callback argument slots and native handles" );
    engine->callbacks++;
}
} // namespace vrdg

int main()
{
    using namespace vrdg;
    AccessEngine test;
    engine = &test;
    State state;
    Compile( &state, &test );
    Check( state.tasks[3].uses.size() == 3 && state.tasks[3].uses[0].flags == ( VRDG_READ | VRDG_STORAGE | VRDG_INDIRECT ) &&
           !state.tasks[3].uses[1].flags, "named and bound aliases merge compatible read roles" );
    const ResourceUse expected[] = {
        { 0, VRDG_TYPE_FB, VRDG_GRAPHICS | VRDG_WRITE | VRDG_COLOR | VRDG_DISCARD },
        { 1, VRDG_TYPE_FB, VRDG_GRAPHICS | VRDG_WRITE | VRDG_DEPTH | VRDG_DISCARD },
        { 3, VRDG_TYPE_BUFFER, VRDG_GRAPHICS | VRDG_WRITE | VRDG_STORAGE | VRDG_DISCARD },
        { 0, VRDG_TYPE_FB, VRDG_COMPUTE | VRDG_READ | VRDG_SAMPLED },
        { 1, VRDG_TYPE_FB, VRDG_COMPUTE | VRDG_READ | VRDG_SAMPLED },
        { 2, VRDG_TYPE_FB, VRDG_COMPUTE | VRDG_READ | VRDG_WRITE | VRDG_STORAGE },
        { 3, VRDG_TYPE_BUFFER, VRDG_COMPUTE | VRDG_READ | VRDG_COPY },
        { 4, VRDG_TYPE_BUFFER, VRDG_COMPUTE | VRDG_WRITE | VRDG_COPY | VRDG_DISCARD },
        { 3, VRDG_TYPE_BUFFER, VRDG_GRAPHICS | VRDG_READ | VRDG_STORAGE | VRDG_INDIRECT },
        { 1, VRDG_TYPE_FB, VRDG_GRAPHICS | VRDG_READ | VRDG_DEPTH }
    };
    for( int run = 0; run < 2; run++ ) {
        test.transitions.clear();
        Run( &state, &test );
        Check( test.transitions.size() == sizeof( expected ) / sizeof( expected[0] ), "one transition per normalized access" );
        for( size_t i = 0; i < test.transitions.size(); i++ )
            Check( test.transitions[i].id == expected[i].id && test.transitions[i].type == expected[i].type && test.transitions[i].flags == expected[i].flags,
                   "engine receives operation, role, discard and task flags in execution order" );
    }
    Check( test.callbacks == 2, "typed accesses run on each execution" );

    AccessEngine conflict;
    conflict.conflicting = true;
    State rejected;
    bool threw = false;
    try { Compile( &rejected, &conflict ); }
    catch ( const std::logic_error& error ) {
        threw = true;
        std::string text = error.what();
        Check( text.find( "ConflictingRoles" ) != std::string::npos && text.find( "resource 0 \"color\"" ) != std::string::npos &&
               text.find( "color" ) != std::string::npos && text.find( "storage" ) != std::string::npos, "role conflict identifies task, resource and combined roles" );
    }
    Check( threw && !rejected.compiled && conflict.transitions.empty(), "bound write role conflicts fail before execution" );
    std::puts( "[PASS] Access roles, compatible aliases, native arguments and role conflicts" );
    return 0;
}
