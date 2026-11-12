#include <cstdio>
#include <cstdlib>
#include "vrdg.h"

void Check( bool condition, const char* message )
{
    if ( !condition ) {
        std::fprintf( stderr, "[FAIL] %s\n", message );
        std::exit( 1 );
    }
}

void CheckText( const std::string& actual, const std::string& expected )
{
    if ( actual != expected ) {
        std::fprintf( stderr, "Actual:\n%sExpected:\n%s", actual.c_str(), expected.c_str() );
        Check( false, "deterministic graph snapshot" );
    }
}

class SnapshotEngine : public vrdg::EngineInterface
{
public:
    uint64_t config = 0, size = 16;
    int builds = 0, resolves = 0, prepares = 0, callbacks = 0;
    bool failBuild = false, failResolve = false, badID = false;

    void HashGraphConfigOptions( uint64_t& a, uint64_t& b ) override { a = config; b = 0; }
    void Build( vrdg::State* s ) override
    {
        builds++;
        for( int i = 0; i < 3; i++ ) {
            vrdg::Resource resource;
            resource.id = i;
            resource.name = i == 0 ? "scratch" : i == 1 ? "external" : "unused";
            resource.type = i == 1 ? vrdg::VRDG_TYPE_FB : vrdg::VRDG_TYPE_BUFFER;
            resource.lifetime = i == 0 ? vrdg::VRDG_TRANSIENT : i == 1 ? vrdg::VRDG_IMPORTED : vrdg::VRDG_PERSISTENT;
            resource.declarationTask = 0;
            resource.resolveDescriptor = [i]( vrdg::State*, vrdg::EngineInterface* ei, vrdg::Task*, vrdg::Resource* r ) {
                auto e = static_cast< SnapshotEngine* >( ei );
                e->resolves++;
                if ( i == 1 && e->failResolve ) throw std::runtime_error( "descriptor failure" );
                r->buffer = vrdg::BufferDesc( e->size, 3, 4 );
                r->fb = vrdg::FBDesc( 64, 32, 5, 2, 1ULL << 40 );
            };
            s->resources.push_back( resource );
        }
        if ( failBuild ) throw std::runtime_error( "build failure" );
        vrdg::Task task;
        task.name = "Produce";
        task.source = "graph.cpp:10";
        task.flags = vrdg::VRDG_GRAPHICS;
        task.uses = { { 0, vrdg::VRDG_TYPE_BUFFER, vrdg::VRDG_WRITE | vrdg::VRDG_DISCARD | vrdg::VRDG_STORAGE } };
        task.callback = [this]( vrdg::State*, vrdg::EngineInterface*, vrdg::Task* ) { callbacks++; };
        s->tasks.push_back( task );
        task.name = "Consume";
        task.source = "graph.cpp:20";
        task.flags = vrdg::VRDG_COMPUTE;
        task.uses = {
            { badID ? 999ULL : 0ULL, vrdg::VRDG_TYPE_BUFFER, vrdg::VRDG_READ | vrdg::VRDG_STORAGE },
            { 0, vrdg::VRDG_TYPE_BUFFER, vrdg::VRDG_READ | vrdg::VRDG_INDIRECT },
            { 1, vrdg::VRDG_TYPE_FB, vrdg::VRDG_READ | vrdg::VRDG_SAMPLED }
        };
        s->tasks.push_back( task );
    }
    void PrepareResources( vrdg::State* s ) override { prepares++; vrdg::EngineInterface::PrepareResources( s ); }
};

int main()
{
    using namespace vrdg;
    State empty;
    CheckText( Describe( empty ), "graph compiled=false descriptors=unresolved tasks=0 resources=0\n" );
    EngineInterface emptyEngine;
    Compile( &empty, &emptyEngine );
    Run( &empty, &emptyEngine );
    CheckText( Describe( empty ), "graph compiled=true descriptors=resolved tasks=0 resources=0\n" );

    SnapshotEngine test;
    State state;
    Compile( &state, &test );
    const std::string tasks =
        "task 0 \"Produce\" graphics source=\"graph.cpp:10\"\n"
        "  arg 0 resource=0 type=buffer access=write|discard|storage\n"
        "task 1 \"Consume\" compute source=\"graph.cpp:20\"\n"
        "  arg 0 resource=0 type=buffer access=read|storage|indirect\n"
        "  arg 1 resource=0 type=buffer access=none merged_into=0\n"
        "  arg 2 resource=1 type=texture access=read|sampled\n";
    CheckText( Describe( state ),
        "graph compiled=true descriptors=unresolved tasks=2 resources=3\n"
        "resource 0 \"scratch\" buffer transient declared=0 first=0 last=1 descriptor=unresolved\n"
        "resource 1 \"external\" texture imported declared=0 first=1 last=1 descriptor=unresolved\n"
        "resource 2 \"unused\" buffer persistent declared=0 first=-1 last=-1 descriptor=unresolved\n" + tasks );
    Check( !test.resolves && !test.prepares && !test.callbacks && !state.descriptorsResolved, "description does not resolve descriptors or run engine code" );
    Run( &state, &test );
    const std::string snapshot = Describe( state );
    CheckText( snapshot,
        "graph compiled=true descriptors=resolved tasks=2 resources=3\n"
        "resource 0 \"scratch\" buffer transient declared=0 first=0 last=1 size=16 format=3 flags=4\n"
        "resource 1 \"external\" texture imported declared=0 first=1 last=1 width=64 height=32 format=5 mips=2 flags=1099511627776\n"
        "resource 2 \"unused\" buffer persistent declared=0 first=-1 last=-1 size=16 format=3 flags=4\n" + tasks );
    CheckText( Describe( state ), snapshot );
    Compile( &state, &test );
    CheckText( Describe( state ), snapshot );
    Check( test.builds == 1 && test.resolves == 3 && test.prepares == 1 && test.callbacks == 2 && state.descriptorsResolved,
           "description and cached compilation preserve resolved state and engine counts" );

    test.size = 1ULL << 34;
    test.failResolve = true;
    bool threw = false;
    try { Run( &state, &test ); }
    catch ( const std::runtime_error& ) { threw = true; }
    std::string partial = Describe( state );
    Check( threw && state.compiled && !state.descriptorsResolved && partial.find( "descriptors=unresolved" ) != std::string::npos &&
           partial.find( " size=" ) == std::string::npos && partial.find( " width=" ) == std::string::npos,
           "partial resolution cannot present stale or mixed descriptors as current" );
    Check( test.resolves == 5 && test.prepares == 1 && test.callbacks == 2, "description does not retry failed resolution" );
    test.failResolve = false;
    Run( &state, &test );
    Check( Describe( state ).find( "size=17179869184" ) != std::string::npos && state.descriptorsResolved, "retry exposes current 64-bit descriptors without rebuilding" );

    test.config++;
    test.badID = true;
    threw = false;
    try { Compile( &state, &test ); }
    catch ( const std::logic_error& ) { threw = true; }
    partial = Describe( state );
    Check( threw && !state.compiled && !state.descriptorsResolved && partial.find( "resource=999 type=buffer access=read|storage undeclared" ) != std::string::npos,
           "failed compilation still describes invalid resource IDs safely" );
    test.badID = false;
    Compile( &state, &test );
    Run( &state, &test );
    test.config++;
    test.failBuild = true;
    threw = false;
    try { Compile( &state, &test ); }
    catch ( const std::runtime_error& ) { threw = true; }
    partial = Describe( state );
    Check( threw && partial.find( "compiled=false descriptors=unresolved tasks=0 resources=3" ) != std::string::npos &&
           partial.find( "declared=0 first=-1 last=-1 descriptor=unresolved" ) != std::string::npos, "partial builds can describe resources whose declaration task is absent" );

    Task escaped;
    escaped.name = "line\n\"quoted\"\\\t\r\x01";
    escaped.source = "path\\file.cpp:7";
    escaped.uses.push_back( { 999, VRDG_TYPE_BUFFER, 1ULL << 63 } );
    State unusual;
    unusual.tasks.push_back( escaped );
    partial = Describe( unusual );
    Check( partial.find( "\"line\\n\\\"quoted\\\"\\\\\\t\\r\\x01\"" ) != std::string::npos &&
           partial.find( "source=\"path\\\\file.cpp:7\"" ) != std::string::npos &&
           partial.find( "unknown=9223372036854775808 undeclared" ) != std::string::npos, "quoted names, source paths and unknown flags remain unambiguous" );
    std::puts( "[PASS] Graph descriptions, descriptor status, partial states and host text" );
    return 0;
}
