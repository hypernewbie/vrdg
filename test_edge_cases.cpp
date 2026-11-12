#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

struct BufferObject { uint64_t value = 0; };
struct TextureObject { uint64_t value = 0; };
typedef BufferObject* BufferHandle;
typedef TextureObject* TextureHandle;
#define VRDG_BUFFER BufferHandle
#define VRDG_FRAMEBUFFER TextureHandle

struct CpuScope
{
    CpuScope( const char* name );
    ~CpuScope();
};
void ProfileBegin( uint64_t id );
void ProfileEnd( uint64_t id );
#define MICROPROFILE_SCOPEI(group, name, color) CpuScope cpuScope( name )
#define VRDG_GPU_PROFILE_BEGIN(id) ProfileBegin( id )
#define VRDG_GPU_PROFILE_END(id) ProfileEnd( id )
#include "vrdg.h"

struct Settings
{
    vrdg::BufferDesc buffer;
    vrdg::FBDesc fb;
};
const Settings& SettingsFor( vrdg::EngineInterface* ei, vrdg::Task* task );
#include "test_edge_cases.h"

///> Graphics_Task( DeclareEarly )
///>     Create_Buffer( seed, std::max( (uint64_t)16, SettingsFor( ei, task ).buffer.size ), SettingsFor( ei, task ).buffer.format, SettingsFor( ei, task ).buffer.flags )
///>     Create_TextureFB( imported, SettingsFor( ei, task ).fb.width, SettingsFor( ei, task ).fb.height, SettingsFor( ei, task ).fb.format, SettingsFor( ei, task ).fb.mips, SettingsFor( ei, task ).fb.flags )
///>     Lifetime( imported, Imported )
///> End_Task

///> Graphics_Task( Seed )
///>     Write_Buffer( seed, Discard )
///>     Func( Seed_Func )
///> End_Task

// A task can access an earlier resource and declare resources for later tasks.
///> Graphics_Task( DeclareLater )
///>     Read_Buffer( seed )
///>     Create_Buffer( work, SettingsFor( ei, task ).buffer.size + 8, SettingsFor( ei, task ).buffer.format, SettingsFor( ei, task ).buffer.flags )
///>     Create_TextureFB( output, SettingsFor( ei, task ).fb.width, SettingsFor( ei, task ).fb.height, SettingsFor( ei, task ).fb.format, SettingsFor( ei, task ).fb.mips, SettingsFor( ei, task ).fb.flags )
///>     Create_Buffer( unused, 16, 0, 0 )
///>     Func( DeclareLater_Func )
///> End_Task

///> Compute_Task( Mixed )
///>     Read_Buffer( first )
///>     Read_TextureFB( imported )
///>     Write_Buffer( second )
///>     ReadWrite_Buffer( work )
///>     Read_Buffer( third )
///>     Write_TextureFB( output, Discard )
///>     CPU_Profile( MixedCpu )
///>     GPU_Profile( MixedGpu )
///>     Func( Mixed_Func )
///> End_Task

///> Graphics_Task( Readback )
///>     Read_TextureFB( output )
///>     Func( Readback_Func )
///> End_Task

void Check( bool condition, const char* message )
{
    if ( !condition ) {
        std::fprintf( stderr, "[FAIL] %s\n", message );
        std::exit( 1 );
    }
}

class EdgeEngine : public vrdg::EngineInterface
{
public:
    Settings early, later;
    BufferObject buffers[2][5];
    TextureObject textures[2][5];
    int frame = 0, builds = 0, resolves = 0;
    bool prepared = false, failResolve = false, failBuild = false, trim = false;
    uint64_t hash0 = 0, hash1 = 0;
    std::vector< std::string > events;
    std::vector< uint64_t > begins, ends;

    void HashGraphConfigOptions( uint64_t& a, uint64_t& b ) override { a = hash0; b = hash1; }

    void Build( vrdg::State* s ) override
    {
        builds++;
        vrdg::Buffer seed, work, unused;
        vrdg::FrameBuffer imported, output;
        vrdg::DeclareEarly( s, seed, imported );
        if ( failBuild )
            throw std::runtime_error( "mock build failure" );
        vrdg::Seed( s, seed );
        if ( trim )
            return;
        vrdg::DeclareLater( s, seed, work, output, unused );
        vrdg::Mixed( s, seed, imported, seed, work, seed, output );
        vrdg::Readback( s, output );
    }

    void CreateBuffer( vrdg::Buffer id, const std::string&, uint64_t, uint32_t, uint32_t ) override
    {
        Check( prepared && ( id == 0 || id == 2 ), "only used transient buffers are created during preparation" );
        buffers[frame][id].value = id == 2 ? 22 : 0;
        events.push_back( "createB" + std::to_string( id ) );
    }

    void CreateFB( vrdg::FrameBuffer id, const std::string&, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t ) override
    {
        Check( prepared && id == 3, "imported and unused objects are not created" );
        textures[frame][id].value = 0;
        events.push_back( "createF3" );
    }

    void PrepareResources( vrdg::State* s ) override
    {
        Check( !prepared && resolves == ( trim ? 8 : 16 ), "all descriptor expressions resolve before preparation" );
        prepared = true;
        events.push_back( "prepare" );
        auto& r = s->resources;
        Check( r.size() == ( trim ? 2 : 5 ), "resource table follows actual topology" );
        Check( r[0].buffer == vrdg::BufferDesc( std::max( ( uint64_t ) 16, early.buffer.size ), early.buffer.format, early.buffer.flags ), "nested expression and all buffer descriptor fields" );
        Check( r[1].fb == early.fb, "all imported texture descriptor fields" );
        Check( r[0].firstUse == 1 && r[0].lastUse == ( trim ? 1 : 3 ), "rebuild recomputes lifetime of retained resource" );
        if ( trim ) {
            Check( r[1].firstUse == -1 && r[1].lastUse == -1, "removed accesses leave no stale interval" );
        }
        else {
            Check( r[2].buffer == vrdg::BufferDesc( later.buffer.size + 8, later.buffer.format, later.buffer.flags ), "later declaration uses its own task context" );
            Check( r[3].fb == later.fb, "all output texture descriptor fields" );
            Check( r[2].firstUse == 3 && r[2].lastUse == 3 && r[3].firstUse == 3 && r[3].lastUse == 4, "declarations do not extend access intervals" );
            Check( r[4].firstUse == -1 && r[4].lastUse == -1, "unused declaration remains unused" );
        }
        vrdg::EngineInterface::PrepareResources( s );
        textures[frame][1].value = 100 + frame; // Bind an external object.
    }

    void ActivateResource( vrdg::State*, const vrdg::Resource& r, uint64_t flags ) override
    {
        Check( prepared, "activation follows preparation" );
        events.push_back( "activate" + std::to_string( r.id ) + ":" + std::to_string( flags ) );
    }

    void TransitionBuffer( vrdg::Buffer id, uint64_t flags ) override
    {
        events.push_back( "transitionB" + std::to_string( id ) + ":" + std::to_string( flags ) );
    }

    void TransitionFB( vrdg::FrameBuffer id, uint64_t flags ) override
    {
        events.push_back( "transitionF" + std::to_string( id ) + ":" + std::to_string( flags ) );
    }

    BufferHandle GetEngineBufferObject( vrdg::Buffer id ) override
    {
        Check( prepared && ( id == 0 || id == 2 ), "buffer getter receives buffer ID" );
        events.push_back( "getB" + std::to_string( id ) );
        return &buffers[frame][id];
    }

    TextureHandle GetEngineFBObject( vrdg::FrameBuffer id ) override
    {
        Check( prepared && ( id == 1 || id == 3 ), "texture getter receives texture ID" );
        events.push_back( "getF" + std::to_string( id ) );
        return &textures[frame][id];
    }

    void ReleaseBuffer( vrdg::Buffer id ) override
    {
        buffers[frame][id].value = 0;
        events.push_back( "releaseB" + std::to_string( id ) );
    }

    void ReleaseFB( vrdg::FrameBuffer id ) override
    {
        Check( id == 3, "external ownership survives last use" );
        textures[frame][id].value = 0;
        events.push_back( "releaseF3" );
    }

    void ResetRun()
    {
        prepared = false;
        resolves = 0;
        events.clear();
        begins.clear();
        ends.clear();
    }
};

static EdgeEngine* engine = nullptr;

const Settings& SettingsFor( vrdg::EngineInterface* ei, vrdg::Task* task )
{
    EdgeEngine* e = static_cast< EdgeEngine* >( ei );
    Check( !e->prepared, "descriptor evaluation precedes engine preparation" );
    e->resolves++;
    if ( task->name == "DeclareEarly" )
        return e->early;
    Check( task->name == "DeclareLater", "descriptor receives its declaration task" );
    if ( e->failResolve ) {
        e->failResolve = false;
        throw std::runtime_error( "mock descriptor failure" );
    }
    return e->later;
}

CpuScope::CpuScope( const char* name )
{
    Check( std::strcmp( name, "MixedCpu" ) == 0, "CPU profile name" );
    engine->events.push_back( "cpuBegin" );
}
CpuScope::~CpuScope() { engine->events.push_back( "cpuEnd" ); }
void ProfileBegin( uint64_t id ) { engine->begins.push_back( id ); engine->events.push_back( "gpuBegin" ); }
void ProfileEnd( uint64_t id ) { engine->ends.push_back( id ); engine->events.push_back( "gpuEnd" ); }

namespace vrdg {
void Seed_Func( BufferHandle seed )
{
    Check( seed == &engine->buffers[engine->frame][0], "seed receives current native pointer" );
    seed->value = 11 + engine->frame;
    engine->events.push_back( "seed" );
}
void DeclareLater_Func( BufferHandle seed )
{
    Check( seed->value == ( uint64_t ) 11 + engine->frame && engine->prepared, "declaration task retains its actual input and callback" );
    engine->events.push_back( "later" );
}
void Mixed_Func( BufferHandle first, TextureHandle imported, BufferHandle second, BufferHandle work, BufferHandle third, TextureHandle output )
{
    Check( first == &engine->buffers[engine->frame][0] && first == second && first == third, "three interleaved aliases retain argument positions" );
    Check( work == &engine->buffers[engine->frame][2] && imported == &engine->textures[engine->frame][1] && output == &engine->textures[engine->frame][3], "distinct native handle types and trailing arguments" );
    Check( first->value == ( uint64_t ) 11 + engine->frame && work->value == 22, "resources survive until their actual last callback" );
    output->value = first->value + work->value + imported->value;
    engine->events.push_back( "mixed" );
}
void Readback_Func( TextureHandle output )
{
    Check( output->value == ( uint64_t ) 133 + 2 * engine->frame, "producer output reaches consumer before release" );
    engine->events.push_back( "readback" );
}
} // namespace vrdg

void CheckEvents( const std::vector< std::string >& actual, const std::vector< std::string >& expected )
{
    if ( actual == expected )
        return;
    for( size_t i = 0; i < actual.size() || i < expected.size(); i++ )
        std::fprintf( stderr, "%zu: %s / expected %s\n", i, i < actual.size() ? actual[i].c_str() : "<missing>", i < expected.size() ? expected[i].c_str() : "<missing>" );
    Check( false, "edge case event ordering" );
}

void RunMixed( vrdg::State& state, EdgeEngine& test )
{
    test.ResetRun();
    vrdg::Run( &state, &test );
    CheckEvents( test.events, {
        "prepare", "createB0", "createB2", "createF3",
        "activate0:22", "transitionB0:22", "getB0", "seed",
        "transitionB0:5", "getB0", "later",
        "activate1:9", "activate2:11", "activate3:26",
        "transitionB0:11", "transitionF1:9", "transitionB2:11", "transitionF3:26",
        "cpuBegin", "gpuBegin", "getB0", "getF1", "getB0", "getB2", "getB0", "getF3", "mixed", "gpuEnd", "cpuEnd",
        "releaseB0", "releaseB2", "transitionF3:5", "getF3", "readback", "releaseF3"
    } );
    Check( aeRenderGraph_GetGpuProfilePassCount() == 1, "one GPU profile table entry" );
    auto table = aeRenderGraph_GetGpuProfilePasses();
    Check( std::strcmp( table[0].first, "MixedGpu" ) == 0 && test.begins.size() == 1 && test.ends.size() == 1 &&
           test.begins[0] == table[0].second && test.ends[0] == table[0].second, "runtime profile IDs match exported table" );
}

void TestGeneratedGraph()
{
    EdgeEngine test;
    engine = &test;
    test.early.buffer = vrdg::BufferDesc( 4, 2, 3 );
    test.early.fb = vrdg::FBDesc( 32, 24, 4, 2, 1ULL << 40 );
    test.later.buffer = vrdg::BufferDesc( 80, 5, 6 );
    test.later.fb = vrdg::FBDesc( 64, 48, 7, 3, 1ULL << 45 );
    vrdg::State state;
    vrdg::Compile( &state, &test );
    Check( std::strstr( state.tasks[0].source, "test_edge_cases.cpp:" ) != nullptr, "generated task retains source location" );
    RunMixed( state, test );

    // Change every descriptor field and all native pointers, without rebuilding.
    test.frame = 1;
    test.early.buffer = vrdg::BufferDesc( ( 1ULL << 34 ) + 1, 12, 13 );
    test.early.fb = vrdg::FBDesc( 320, 240, 14, 4, 1ULL << 50 );
    test.later.buffer = vrdg::BufferDesc( ( 1ULL << 35 ) + 3, 15, 16 );
    test.later.fb = vrdg::FBDesc( 640, 480, 17, 5, 1ULL << 55 );
    vrdg::Compile( &state, &test );
    Check( test.builds == 1, "descriptor changes do not rebuild topology" );
    RunMixed( state, test );

    test.ResetRun();
    test.failResolve = true;
    bool threw = false;
    try { vrdg::Run( &state, &test ); }
    catch ( const std::runtime_error& error ) { threw = true; Check( std::strcmp( error.what(), "mock descriptor failure" ) == 0, "descriptor exception reaches host unchanged" ); }
    Check( threw && !test.prepared && test.events.empty() && test.resolves > 8, "descriptor failure prevents preparation and passes" );
    RunMixed( state, test );

    // Changing only the second hash word must remove old tasks and intervals.
    test.hash1++;
    test.trim = true;
    vrdg::Compile( &state, &test );
    Check( test.builds == 2 && state.tasks.size() == 2, "second hash word rebuilds a smaller topology" );
    test.ResetRun();
    vrdg::Run( &state, &test );
    CheckEvents( test.events, { "prepare", "createB0", "activate0:22", "transitionB0:22", "getB0", "seed", "releaseB0" } );

    test.hash0++;
    test.trim = false;
    test.failBuild = true;
    threw = false;
    try { vrdg::Compile( &state, &test ); }
    catch ( const std::runtime_error& error ) { threw = true; Check( std::strcmp( error.what(), "mock build failure" ) == 0, "build exception reaches host unchanged" ); }
    Check( threw && !state.compiled, "partially built graph is not executable" );
    test.ResetRun();
    threw = false;
    try { vrdg::Run( &state, &test ); }
    catch ( const std::logic_error& ) { threw = true; }
    Check( threw && test.events.empty() && !test.resolves, "failed build blocks all execution" );
    test.failBuild = false;
    vrdg::Compile( &state, &test );
    Check( test.builds == 4 && state.resources.size() == 5, "same hash retries build without partial declarations" );
    RunMixed( state, test );
    std::puts( "[PASS] Native handles, argument slots, descriptors, profiling and rebuilds" );
}

// Exercise runtime validation independently of the generated happy-path graph.
class AccessEngine : public vrdg::EngineInterface
{
public:
    std::vector< vrdg::ResourceUse > accesses;
    vrdg::ResourceLifetime lifetime = vrdg::VRDG_TRANSIENT;
    bool empty = false;
    int builds = 0, resolves = 0, prepares = 0, creates = 0;
    int activates = 0, transitions = 0, callbacks = 0, releases = 0;
    uint64_t transitionFlags = 0;

    void HashGraphConfigOptions( uint64_t& a, uint64_t& b ) override { a = b = 0; }

    void Build( vrdg::State* state ) override
    {
        builds++;
        if ( empty )
            return;
        vrdg::Resource resource;
        resource.name = "scratch";
        resource.lifetime = lifetime;
        resource.declarationTask = 0;
        resource.resolveDescriptor = []( vrdg::State*, vrdg::EngineInterface* ei, vrdg::Task*, vrdg::Resource* r ) {
            static_cast< AccessEngine* >( ei )->resolves++;
            r->buffer = vrdg::BufferDesc( 16, 0, 0 );
        };
        state->resources.push_back( resource );
        vrdg::Task task;
        task.name = "Access";
        task.source = "validation.cpp:17";
        task.flags = vrdg::VRDG_COMPUTE;
        task.uses = accesses;
        task.callback = [this]( vrdg::State*, vrdg::EngineInterface*, vrdg::Task* ) { callbacks++; };
        state->tasks.push_back( task );
    }

    void PrepareResources( vrdg::State* s ) override { prepares++; vrdg::EngineInterface::PrepareResources( s ); }
    void CreateBuffer( vrdg::Buffer, const std::string&, uint64_t, uint32_t, uint32_t ) override { creates++; }
    void ActivateResource( vrdg::State*, const vrdg::Resource&, uint64_t ) override { activates++; }
    void TransitionBuffer( vrdg::Buffer, uint64_t flags ) override { transitions++; transitionFlags = flags; }
    void ReleaseBuffer( vrdg::Buffer ) override { releases++; }
};

void TestAccessValidation()
{
    using namespace vrdg;
    const ResourceUse read = { 0, VRDG_TYPE_BUFFER, VRDG_READ };
    const ResourceUse write = { 0, VRDG_TYPE_BUFFER, VRDG_WRITE };
    const ResourceUse readWrite = { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_WRITE };
    const ResourceUse discard = { 0, VRDG_TYPE_BUFFER, VRDG_WRITE | VRDG_DISCARD };
    const std::vector< ResourceUse > invalid[] = {
        { read, discard }, { discard, read },
        { write, discard }, { discard, write },
        { readWrite, discard }, { discard, readWrite },
        { { 1, VRDG_TYPE_BUFFER, VRDG_READ } },
        { { 0, VRDG_TYPE_FB, VRDG_READ } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_WRITE | VRDG_SAMPLED } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_COLOR } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_WRITE | VRDG_INDIRECT } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_WRITE | VRDG_COPY } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_STORAGE }, { 0, VRDG_TYPE_BUFFER, VRDG_WRITE | VRDG_COPY } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_DISCARD } },
        { { 0, VRDG_TYPE_BUFFER, VRDG_READ | VRDG_WRITE | VRDG_DISCARD } }
    };
    for( auto& accesses : invalid ) {
        AccessEngine test;
        test.accesses = accesses;
        State state;
        bool threw = false;
        try { Compile( &state, &test ); }
        catch ( const std::logic_error& error ) {
            threw = true;
            std::string text = error.what();
            Check( text.find( "validation.cpp:17: task 0 \"Access\", resource " ) != std::string::npos, "runtime diagnostic includes source and task" );
            Check( text.find( accesses[0].id == 1 ? "resource 1:" : "resource 0 \"scratch\":" ) != std::string::npos, "runtime diagnostic identifies declared or missing resource" );
        }
        Check( threw && !state.compiled, "invalid access graph rejected" );
        threw = false;
        try { Run( &state, &test ); }
        catch ( const std::logic_error& ) { threw = true; }
        Check( threw && !test.resolves && !test.prepares && !test.callbacks && !test.transitions, "rejected accesses cannot reach engine execution" );

        test.accesses = { read, write, readWrite, read };
        Compile( &state, &test ); // Same hash as the failed compile.
        Run( &state, &test );
        Check( test.builds == 2 && test.resolves == 1 && test.prepares == 1 && test.creates == 1 &&
               test.activates == 1 && test.callbacks == 1 && test.transitions == 1 && test.releases == 1 &&
               test.transitionFlags == ( VRDG_COMPUTE | VRDG_READ | VRDG_WRITE ), "retry combines four aliases into one transition and release" );
    }

    // Repeated writes with the same policy are valid, including discard writes.
    for( auto access : { read, write, discard } ) {
        AccessEngine test;
        test.accesses = { access, access, access };
        State state;
        Compile( &state, &test );
        Run( &state, &test );
        Run( &state, &test );
        Check( test.transitions == 2 && test.activates == 2 && test.callbacks == 2 && test.releases == 2 &&
               test.transitionFlags == ( VRDG_COMPUTE | access.flags ), "repeated accesses keep their intent across executions" );
    }
    std::puts( "[PASS] Access conflicts, invalid IDs/types and duplicate transitions" );
}

void TestEmptyGraphsAndOwnership()
{
    using namespace vrdg;
    AccessEngine empty;
    empty.empty = true;
    State emptyState;
    bool threw = false;
    try { Run( &emptyState, &empty ); }
    catch ( const std::logic_error& ) { threw = true; }
    Check( threw && !empty.prepares, "uncompiled graph cannot start preparation" );
    Compile( &emptyState, &empty );
    Compile( &emptyState, &empty );
    Run( &emptyState, &empty );
    Run( &emptyState, &empty );
    Check( empty.builds == 1 && empty.prepares == 2 && !empty.resolves && !empty.callbacks, "empty zero-hash graph is cached and can run repeatedly" );

    AccessEngine unused;
    State unusedState;
    Compile( &unusedState, &unused );
    Run( &unusedState, &unused );
    Check( unused.resolves == 1 && unused.prepares == 1 && unused.callbacks == 1 && !unused.creates &&
           !unused.activates && !unused.transitions && !unused.releases, "declaration-only graph resolves metadata without allocating or accessing resources" );

    for( auto lifetime : { VRDG_TRANSIENT, VRDG_PERSISTENT, VRDG_IMPORTED } ) {
        AccessEngine test;
        test.lifetime = lifetime;
        test.accesses = { { 0, VRDG_TYPE_BUFFER, VRDG_READ } };
        State state;
        Compile( &state, &test );
        Run( &state, &test );
        Run( &state, &test );
        Check( test.callbacks == 2 && test.transitions == 2 && test.activates == 2, "all lifetimes retain their actual accesses" );
        Check( test.creates == ( lifetime == VRDG_TRANSIENT ? 2 : 0 ) &&
               test.releases == ( lifetime == VRDG_TRANSIENT ? 2 : 0 ), "persistent and imported buffers stay under engine ownership" );
    }
    std::puts( "[PASS] Empty graphs, unused declarations and buffer ownership" );
}

void TestDescriptorChanges()
{
    // Engines use descriptor equality to decide whether a cached object needs replacement.
    const vrdg::BufferDesc buffer( 1ULL << 34, 7, 1u << 31 );
    const vrdg::BufferDesc changedBuffers[] = {
        { 1ULL << 35, 7, 1u << 31 }, { 1ULL << 34, 8, 1u << 31 }, { 1ULL << 34, 7, 0 }
    };
    Check( buffer == vrdg::BufferDesc( 1ULL << 34, 7, 1u << 31 ), "unchanged buffer descriptor retains cache identity" );
    for( auto& changed : changedBuffers )
        Check( !( buffer == changed ) && !( changed == buffer ), "each buffer field affects cache identity" );

    const vrdg::FBDesc fb( 64, 48, 7, 4, 1ULL << 48 );
    const vrdg::FBDesc changedFBs[] = {
        { 65, 48, 7, 4, 1ULL << 48 }, { 64, 49, 7, 4, 1ULL << 48 },
        { 64, 48, 8, 4, 1ULL << 48 }, { 64, 48, 7, 5, 1ULL << 48 }, { 64, 48, 7, 4, 1ULL << 49 }
    };
    Check( fb == vrdg::FBDesc( 64, 48, 7, 4, 1ULL << 48 ), "unchanged texture descriptor retains cache identity" );
    for( auto& changed : changedFBs )
        Check( !( fb == changed ) && !( changed == fb ), "each texture field affects cache identity, including high flag bits" );
    std::puts( "[PASS] Descriptor changes invalidate cache identity one field at a time" );
}

int main()
{
    TestGeneratedGraph();
    TestAccessValidation();
    TestEmptyGraphsAndOwnership();
    TestDescriptorChanges();
    return 0;
}
