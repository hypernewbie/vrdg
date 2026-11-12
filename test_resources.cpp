#include <cstdio>
#include <cstdlib>
#include "vrdg.h"

uint32_t ResolveWidth( vrdg::EngineInterface* ei );
#include "test_resources.h"

///> Graphics_Task( Declare )
///>     Create_Buffer( scratch, ResolveWidth( ei ) * 4, 1, 2 )
///>     Create_TextureFB( a, ResolveWidth( ei ), 32, 3, 1 )
///>     Create_TextureFB( b, ResolveWidth( ei ), 32, 3, 1, 4 )
///>     Create_TextureFB( history, ResolveWidth( ei ), 32, 3, 1, 4 )
///>     Lifetime( history, Persistent )
///>     Create_TextureFB( external, ResolveWidth( ei ), 32, 3, 1, 4 )
///>     Lifetime( external, Imported )
///>     Create_Buffer( unused, ResolveWidth( ei ), 0, 0 )
///>     Create_TextureFB( overlap, ResolveWidth( ei ), 32, 3, 1, 4 )
///> End_Task

///> Graphics_Task( WriteA )
///>     Write_TextureFB( a, Discard )
///>     Write_Buffer( scratch, Discard )
///>     Func( WriteA_Func )
///> End_Task

///> Graphics_Task( ReadA )
///>     Read_TextureFB( a )
///>     Write_TextureFB( overlap, Discard )
///>     Func( ReadA_Func )
///> End_Task

///> Compute_Task( WriteB )
///>     Read_TextureFB( overlap )
///>     Write_TextureFB( b, Discard )
///>     Func( WriteB_Func )
///> End_Task

///> Graphics_Task( Finish )
///>     Read_TextureFB( b )
///>     ReadWrite_Buffer( scratch )
///>     ReadWrite_TextureFB( history )
///>     Read_TextureFB( external )
///>     Func( Finish_Func )
///> End_Task

// Distinct arguments can refer to the same logical resource at build time.
///> Graphics_Task( Combined )
///>     Read_Buffer( input )
///>     Write_Buffer( output )
///>     Func( Combined_Func )
///> End_Task

// Repeated names in annotations produce one read/write callback argument.
///> Compute_Task( Merged )
///>     Read_TextureFB( history )
///>     Write_TextureFB( history )
///>     Func( Merged_Func )
///> End_Task

///> Graphics_Task( Conflicting )
///>     Read_Buffer( input )
///>     Write_Buffer( output, Discard )
///> End_Task

///> Graphics_Task( DeclareSimple )
///>     Create_TextureFB( texture, ResolveWidth( ei ), 32, 3, 1, 4 )
///> End_Task

///> Graphics_Task( ReadSimple )
///>     Read_TextureFB( texture )
///>     Func( ReadSimple_Func )
///> End_Task

void Check( bool condition, const char* message )
{
    if ( !condition ) {
        std::fprintf( stderr, "[FAIL] %s\n", message );
        std::exit( 1 );
    }
}

bool CanAlias( const vrdg::Resource& a, const vrdg::Resource& b )
{
    return a.lifetime == vrdg::VRDG_TRANSIENT && b.lifetime == vrdg::VRDG_TRANSIENT &&
        a.firstUse >= 0 && b.firstUse >= 0 && ( a.lastUse < b.firstUse || b.lastUse < a.firstUse );
}

class ResourceEngine : public vrdg::EngineInterface
{
public:
    uint32_t width = 64;
    int buildCount = 0, config = 0;
    bool simple = false, conflicting = false;
    bool failHistory = false, historyNeedsInit = false;
    int simpleContents = -1;
    vrdg::FBDesc historyDesc;
    uint64_t objectSerial = 0;
    int historyContents = 0;
    uint64_t objects[7] = {};
    bool activated[7] = {};
    int occupant = -1;
    uint64_t occupantObject = 0;
    std::vector< std::string > events;

    void HashGraphConfigOptions( uint64_t& hash0, uint64_t& hash1 ) override
    {
        // Also exercise the initial all-zero hash, which still needs a build.
        hash0 = config;
        hash1 = 0;
    }

    void Build( vrdg::State* s ) override
    {
        buildCount++;
        if ( simple ) {
            vrdg::FrameBuffer texture;
            vrdg::DeclareSimple( s, texture );
            vrdg::ReadSimple( s, texture );
            return;
        }
        vrdg::Buffer scratch, unused;
        vrdg::FrameBuffer a, b, history, external, overlap;
        vrdg::Declare( s, scratch, a, b, history, external, unused, overlap );
        vrdg::WriteA( s, a, scratch );
        vrdg::ReadA( s, a, overlap );
        vrdg::WriteB( s, overlap, b );
        vrdg::Finish( s, b, scratch, history, external );
        if ( conflicting )
            vrdg::Conflicting( s, scratch, scratch );
        else
            vrdg::Combined( s, scratch, scratch );
        vrdg::Merged( s, history );
    }

    void CreateBuffer( vrdg::Buffer id, const std::string& name, uint64_t size, uint32_t format, uint32_t flags ) override
    {
        Check( id == 0 && name == "scratch", "only used transient buffer is created" );
        Check( size == width * 4 && format == 1 && flags == 2, "buffer descriptor" );
        objects[id] = ++objectSerial;
        events.push_back( "create0" );
    }

    void CreateFB( vrdg::FrameBuffer id, const std::string& name, uint32_t w, uint32_t h, uint32_t format, uint32_t mips, uint64_t flags ) override
    {
        Check( simple || ( id >= 1 && id <= 3 ) || id == 6, "imported resource is never created" );
        Check( w == width && h == 32 && format == 3 && mips == 1, "texture descriptor" );
        Check( flags == ( id == 1 ? 0 : 4 ), "default and explicit texture flags" );
        if ( name == "history" ) {
            if ( failHistory ) {
                failHistory = false;
                throw std::runtime_error( "mock allocation failure" );
            }
            historyNeedsInit = true;
        }
        if ( simple ) simpleContents = -1;
        objects[id] = ++objectSerial;
        events.push_back( "create" + std::to_string( id ) );
    }

    void PrepareResources( vrdg::State* s ) override
    {
        events.push_back( "prepare" );
        if ( simple ) {
            Check( s->resources.size() == 1, "existing annotations support preparation" );
            activated[0] = false;
            vrdg::EngineInterface::PrepareResources( s );
            return;
        }
        Check( s->resources.size() == 7, "all declarations exposed" );
        auto& r = s->resources;
        Check( r[0].firstUse == 1 && r[0].lastUse == 5, "buffer interval excludes declaration" );
        Check( r[1].firstUse == 1 && r[1].lastUse == 2, "first texture interval" );
        Check( r[2].firstUse == 3 && r[2].lastUse == 4, "second texture interval" );
        Check( r[3].firstUse == 4 && r[3].lastUse == 6 && r[3].lifetime == vrdg::VRDG_PERSISTENT, "persistent interval" );
        Check( r[4].firstUse == 4 && r[4].lastUse == 4 && r[4].lifetime == vrdg::VRDG_IMPORTED, "imported interval" );
        Check( r[5].firstUse == -1 && r[5].lastUse == -1, "unused declaration has no interval" );
        Check( r[6].firstUse == 2 && r[6].lastUse == 3, "overlap resource interval" );
        Check( !CanAlias( r[1], r[6] ) && !CanAlias( r[2], r[6] ), "interval endpoints are inclusive" );
        Check( CanAlias( r[1], r[2] ), "disjoint transients can share the mock allocation" );
        Check( !CanAlias( r[0], r[1] ) && !CanAlias( r[0], r[2] ), "overlapping transients cannot share" );
        Check( !CanAlias( r[1], r[3] ) && !CanAlias( r[1], r[4] ) && !CanAlias( r[1], r[5] ), "lifetime classes and unused declarations excluded" );
        for( int i = 0; i < 7; i++ ) {
            Check( r[i].id == ( uint64_t ) i && r[i].declarationTask == 0, "resource identity and declaration task" );
            Check( i == 0 ? r[i].buffer.size == width * 4 : i == 5 ? r[i].buffer.size == width : r[i].fb.width == width, "all descriptors resolved before preparation" );
            activated[i] = false;
        }
        Check( s->tasks[0].uses.empty(), "declaration is not a write" );
        Check( s->tasks[5].uses.size() == 2 && s->tasks[5].uses[0].flags == ( vrdg::VRDG_READ | vrdg::VRDG_WRITE ) &&
               s->tasks[5].uses[1].flags == 0, "combined access retains both callback argument slots" );
        vrdg::EngineInterface::PrepareResources( s );
        if ( !objects[3] || !( historyDesc == r[3].fb ) ) {
            auto& desc = r[3].fb;
            CreateFB( r[3].id, r[3].name, desc.width, desc.height, desc.format, desc.mips, desc.flags );
            historyDesc = desc; // Remember only a successfully created object.
        }
        objects[4] = 900000 + width; // Bind an externally owned object each execution.
    }

    void ActivateResource( vrdg::State* s, const vrdg::Resource& resource, uint64_t flags ) override
    {
        uint64_t id = resource.id;
        Check( !activated[id] && objects[id] != 0, "activation after preparation, once per execution" );
        activated[id] = true;
        events.push_back( "activate" + std::to_string( id ) );
        if ( simple ) {
            simpleContents = 0;
            events.push_back( "initialize0" );
        }
        else if ( id == 3 && historyNeedsInit ) {
            historyContents = 0;
            historyNeedsInit = false;
            events.push_back( "initialize3" );
        }
        else if ( resource.lifetime == vrdg::VRDG_TRANSIENT )
            vrdg::EngineInterface::ActivateResource( s, resource, flags );
        if ( id == 1 || id == 2 ) {
            Check( flags == ( id == 1 ? 22 : 26 ), "alias occupant starts with an explicit discard write" );
            if ( occupant >= 0 ) {
                Check( occupantObject != objects[id], "handoff retains previous physical object identity" );
                if ( id == 1 )
                    Check( occupant == 2, "previous execution's occupant retained" );
                else
                    Check( occupant == 1, "within-execution alias handoff" );
                events.push_back( "handoff" + std::to_string( occupant ) + "to" + std::to_string( id ) );
            }
            occupant = ( int ) id;
            occupantObject = objects[id];
        }
    }

    void Transition( uint64_t id, uint64_t flags )
    {
        Check( activated[id], "activation precedes transition" );
        events.push_back( "transition" + std::to_string( id ) + ":" + std::to_string( flags ) );
    }

    void TransitionBuffer( vrdg::Buffer id, uint64_t flags ) override { Transition( id, flags ); }
    void TransitionFB( vrdg::FrameBuffer id, uint64_t flags ) override { Transition( id, flags ); }

    uint64_t GetObject( uint64_t id )
    {
        Check( objects[id] != 0, "handle exists before callback" );
        events.push_back( "get" + std::to_string( id ) );
        return objects[id];
    }

    VRDG_BUFFER GetEngineBufferObject( vrdg::Buffer id ) override { return GetObject( id ); }
    VRDG_FRAMEBUFFER GetEngineFBObject( vrdg::FrameBuffer id ) override { return GetObject( id ); }

    void Release( uint64_t id )
    {
        Check( simple || id <= 2 || id == 6, "persistent, imported and unused resources are never released" );
        events.push_back( "release" + std::to_string( id ) );
        // Logical release does not forget the physical alias occupant or wait for a GPU.
    }

    void ReleaseBuffer( vrdg::Buffer id ) override { Release( id ); }
    void ReleaseFB( vrdg::FrameBuffer id ) override { Release( id ); }


};

static ResourceEngine* engine = nullptr;

uint32_t ResolveWidth( vrdg::EngineInterface* ei )
{
    ResourceEngine* test = static_cast< ResourceEngine* >( ei );
    test->events.push_back( "resolve" );
    return test->width;
}

namespace vrdg {

void WriteA_Func( VRDG_FRAMEBUFFER a, VRDG_BUFFER scratch )
{
    Check( a == engine->objects[1] && scratch == engine->objects[0], "first callback receives current handles" );
    engine->events.push_back( "writeA" );
}

void ReadA_Func( VRDG_FRAMEBUFFER a, VRDG_FRAMEBUFFER overlap )
{
    Check( a == engine->objects[1] && overlap == engine->objects[6], "read receives current alias object" );
    engine->events.push_back( "readA" );
}

void WriteB_Func( VRDG_FRAMEBUFFER overlap, VRDG_FRAMEBUFFER b )
{
    Check( b == engine->objects[2] && overlap == engine->objects[6], "second callback receives current alias object" );
    engine->events.push_back( "writeB" );
}

void Finish_Func( VRDG_FRAMEBUFFER b, VRDG_BUFFER scratch, VRDG_FRAMEBUFFER history, VRDG_FRAMEBUFFER external )
{
    Check( b == engine->objects[2] && scratch == engine->objects[0] && history == engine->objects[3] && external == engine->objects[4], "read/write and imported handles" );
    engine->historyContents++;
    engine->events.push_back( "finish" );
}

void Combined_Func( VRDG_BUFFER input, VRDG_BUFFER output )
{
    Check( input == output && input == engine->objects[0], "normalized access preserves callback argument order" );
    engine->events.push_back( "combined" );
}

void Merged_Func( VRDG_FRAMEBUFFER history )
{
    Check( history == engine->objects[3], "merged annotations produce one callback argument" );
    engine->events.push_back( "merged" );
}

void ReadSimple_Func( VRDG_FRAMEBUFFER texture )
{
    Check( texture == engine->objects[0] && engine->simpleContents == 0, "engine supplies initial contents for a first read" );
    engine->events.push_back( "simple" );
}

} // namespace vrdg

void CheckEvents( const std::vector< std::string >& actual, const std::vector< std::string >& expected )
{
    if ( actual == expected )
        return;
    for( size_t i = 0; i < actual.size() || i < expected.size(); i++ )
        std::fprintf( stderr, "%zu: %s / expected %s\n", i, i < actual.size() ? actual[i].c_str() : "<missing>", i < expected.size() ? expected[i].c_str() : "<missing>" );
    Check( false, "callback event ordering" );
}

void RunGraph( vrdg::State& state, ResourceEngine& test, bool createHistory, int historyContents )
{
    test.events.clear();
    bool previousOccupant = test.occupant >= 0;
    vrdg::Run( &state, &test );
    std::vector< std::string > expected( 7, "resolve" );
    expected.push_back( "prepare" );
    expected.push_back( "create0" );
    expected.push_back( "create1" );
    expected.push_back( "create2" );
    expected.push_back( "create6" );
    if ( createHistory )
        expected.push_back( "create3" );
    expected.push_back( "activate1" );
    if ( previousOccupant )
        expected.push_back( "handoff2to1" );
    const char* rest[] = {
        "activate0", "transition1:22", "transition0:22", "get1", "get0", "writeA",
        "activate6", "transition1:5", "transition6:22", "get1", "get6", "readA", "release1",
        "activate2", "handoff1to2", "transition6:9", "transition2:26", "get6", "get2", "writeB", "release6",
        "activate3"
    };
    expected.insert( expected.end(), rest, rest + sizeof( rest ) / sizeof( rest[0] ) );
    if ( createHistory ) expected.push_back( "initialize3" );
    const char* finish[] = {
        "activate4", "transition2:5", "transition0:7", "transition3:7", "transition4:5",
        "get2", "get0", "get3", "get4", "finish", "release2",
        "transition0:7", "get0", "get0", "combined", "release0",
        "transition3:11", "get3", "merged"
    };
    expected.insert( expected.end(), finish, finish + sizeof( finish ) / sizeof( finish[0] ) );
    CheckEvents( test.events, expected );
    Check( test.historyContents == historyContents, "persistent contents follow engine reuse/invalidation decisions" );
}

int main()
{
    ResourceEngine test;
    engine = &test;
    vrdg::State state;
    vrdg::Compile( &state, &test );
    Check( test.buildCount == 1, "zero hash still builds once" );
    RunGraph( state, test, true, 1 );
    uint64_t firstA = test.objects[1];
    test.width = 128;
    vrdg::Compile( &state, &test );
    Check( test.buildCount == 1, "resolution excluded from topology hash" );
    RunGraph( state, test, true, 1 );
    Check( test.objects[1] != firstA, "resize changes callback handles without rebuild" );
    uint64_t history = test.objects[3];
    RunGraph( state, test, false, 2 );
    Check( test.objects[3] == history, "persistent object retained on unchanged descriptors" );

    test.config++;
    vrdg::Compile( &state, &test );
    Check( test.buildCount == 2, "topology change rebuilds" );
    RunGraph( state, test, false, 3 );
    Check( test.objects[3] == history, "engine can retain persistent contents across rebuilds" );

    test.width = 256;
    test.failHistory = true;
    test.events.clear();
    bool rejected = false;
    try { vrdg::Run( &state, &test ); }
    catch ( const std::runtime_error& ) { rejected = true; }
    Check( rejected && test.events.back() == "create6", "failed preparation executes no passes" );
    Check( test.objects[3] == history && test.historyDesc.width == 128, "failed resize retains the actual object's descriptor" );
    RunGraph( state, test, true, 1 );
    Check( test.objects[3] != history && test.historyDesc.width == 256, "retry replaces the object with the requested size" );

    test.config++;
    test.conflicting = true;
    rejected = false;
    try { vrdg::Compile( &state, &test ); }
    catch ( const std::logic_error& ) { rejected = true; }
    Check( rejected && !state.compiled, "runtime rejects reading a discarded alias" );
    test.conflicting = false;
    vrdg::Compile( &state, &test );
    Check( state.compiled && test.buildCount == 4, "failed compile cannot populate topology cache" );
    RunGraph( state, test, false, 2 );

    ResourceEngine simple;
    simple.simple = true;
    engine = &simple;
    vrdg::State simpleState;
    vrdg::Compile( &simpleState, &simple );
    for( int i = 0; i < 2; i++ ) {
        simple.events.clear();
        simple.width = 64 + i * 64;
        vrdg::Compile( &simpleState, &simple );
        vrdg::Run( &simpleState, &simple );
        CheckEvents( simple.events, { "resolve", "prepare", "create0", "activate0", "initialize0", "transition0:5", "get0", "simple", "release0" } );
    }
    Check( simple.buildCount == 1, "existing annotations use the same execution path" );
    std::puts( "[PASS] Resource preparation, access, alias and ownership tests" );
    return 0;
}
