#include <algorithm>
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

class DefaultEngine : public vrdg::EngineInterface
{
public:
    vrdg::AttachmentType type = vrdg::VRDG_TYPE_BUFFER;
    vrdg::ResourceLifetime lifetime = vrdg::VRDG_TRANSIENT;
    uint64_t access = vrdg::VRDG_READ;
    uint32_t size = 16, cachedSize = 0;
    bool alias = false, valid = false, importedValid = false;
    int contents = -1, occupant = -1, creates = 0, transitions = 0, callbacks = 0, releases = 0;
    std::vector< int > observed;
    std::vector< std::string > events;

    void Build( vrdg::State* s ) override
    {
        for( int i = 0; i < ( alias ? 2 : 1 ); i++ ) {
            vrdg::Resource resource;
            resource.id = i;
            resource.name = i ? "next" : "initial";
            resource.type = type;
            resource.lifetime = lifetime;
            resource.declarationTask = i;
            resource.resolveDescriptor = []( vrdg::State*, vrdg::EngineInterface* ei, vrdg::Task*, vrdg::Resource* r ) {
                auto e = static_cast< DefaultEngine* >( ei );
                r->buffer = vrdg::BufferDesc( e->size );
                r->fb = vrdg::FBDesc( e->size, 16 );
            };
            s->resources.push_back( resource );
            vrdg::Task task;
            task.name = i ? "Consume" : "First";
            task.source = "initialization.cpp:20";
            task.flags = vrdg::VRDG_GRAPHICS;
            task.uses.push_back( { ( uint64_t ) i, type, alias && !i ? vrdg::VRDG_WRITE | vrdg::VRDG_DISCARD : access } );
            task.callback = [this, i]( vrdg::State*, vrdg::EngineInterface*, vrdg::Task* t ) {
                callbacks++;
                observed.push_back( contents );
                if ( t->uses[0].flags & vrdg::VRDG_WRITE ) {
                    contents = ( t->uses[0].flags & vrdg::VRDG_DISCARD ) ? 99 : contents + 1;
                    valid = true;
                }
                events.push_back( "callback" + std::to_string( i ) );
            };
            s->tasks.push_back( task );
        }
    }

    void Create()
    {
        creates++;
        valid = false;
        contents = -1;
    }
    void CreateBuffer( vrdg::Buffer, const std::string&, uint64_t, uint32_t, uint32_t ) override { Create(); }
    void CreateFB( vrdg::FrameBuffer, const std::string&, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t ) override { Create(); }

    void PrepareResources( vrdg::State* s ) override
    {
        if ( lifetime == vrdg::VRDG_TRANSIENT )
            vrdg::EngineInterface::PrepareResources( s );
        else if ( lifetime == vrdg::VRDG_IMPORTED ) {
            valid = importedValid;
            contents = importedValid ? 77 : -1;
        }
        else if ( cachedSize != size ) {
            Create();
            cachedSize = size;
        }
    }

    void Transition( uint64_t id ) { transitions++; events.push_back( "transition" + std::to_string( id ) ); }
    void TransitionBuffer( vrdg::Buffer id, uint64_t ) override { Transition( id ); }
    void TransitionFB( vrdg::FrameBuffer id, uint64_t ) override { Transition( id ); }
    void ReleaseBuffer( vrdg::Buffer ) override { releases++; }
    void ReleaseFB( vrdg::FrameBuffer ) override { releases++; }
};

class InitializingEngine : public DefaultEngine
{
public:
    bool initializeMissing = true;
    int initializations = 0;

    void ActivateResource( vrdg::State* s, const vrdg::Resource& r, uint64_t flags ) override
    {
        if ( occupant != ( int ) r.id ) {
            if ( alias ) valid = false;
            occupant = ( int ) r.id;
            events.push_back( "handoff" + std::to_string( r.id ) );
        }
        if ( !( flags & vrdg::VRDG_DISCARD ) && !valid ) {
            if ( !initializeMissing || r.lifetime == vrdg::VRDG_IMPORTED )
                throw vrdg::ResourceError( s, r.firstUse, r.id, "initial contents are unavailable" );
            contents = 0;
            valid = true;
            initializations++;
            events.push_back( "initialize" + std::to_string( r.id ) );
        }
    }
};

int main()
{
    using namespace vrdg;
    for( auto type : { VRDG_TYPE_BUFFER, VRDG_TYPE_FB } ) {
        for( uint64_t access : { ( uint64_t ) VRDG_READ, ( uint64_t ) VRDG_WRITE, ( uint64_t ) ( VRDG_READ | VRDG_WRITE ) } ) {
            DefaultEngine test;
            test.type = type;
            test.access = access;
            State state;
            Compile( &state, &test );
            bool threw = false;
            try { Run( &state, &test ); }
            catch ( const std::logic_error& error ) {
                threw = true;
                std::string text = error.what();
                Check( text.find( "initialization.cpp:20: task 0 \"First\", resource 0 \"initial\"" ) != std::string::npos &&
                       text.find( "initial contents" ) != std::string::npos, "first-use error reaches host with task and resource context" );
            }
            Check( threw && !test.transitions && !test.callbacks, "fresh read or preserving write fails before transition and callback" );
        }
        DefaultEngine discard;
        discard.type = type;
        discard.access = VRDG_WRITE | VRDG_DISCARD;
        State state;
        Compile( &state, &discard );
        Run( &state, &discard );
        Run( &state, &discard );
        Check( discard.callbacks == 2 && discard.contents == 99 && discard.releases == 2, "fresh discard writes need no engine initialization" );
    }

    InitializingEngine alias;
    alias.alias = true;
    State aliasState;
    Compile( &aliasState, &alias );
    Run( &aliasState, &alias );
    Check( alias.observed.size() == 2 && alias.observed[1] == 0 && alias.initializations == 1, "new alias reads its initialized contents rather than the previous occupant's data" );
    Check( alias.events == std::vector< std::string >{ "handoff0", "transition0", "callback0", "handoff1", "initialize1", "transition1", "callback1" },
           "initialization follows alias handoff and precedes the consumer transition" );
    alias.events.clear();
    Run( &aliasState, &alias );
    Check( alias.initializations == 2 && alias.observed.back() == 0, "alias initialization remains correct across executions" );

    InitializingEngine history;
    history.lifetime = VRDG_PERSISTENT;
    history.access = VRDG_READ | VRDG_WRITE;
    State historyState;
    Compile( &historyState, &history );
    Run( &historyState, &history );
    Run( &historyState, &history );
    history.size = 32;
    Run( &historyState, &history );
    Check( history.observed == std::vector< int >{ 0, 1, 0 } && history.creates == 2 && history.initializations == 2 && !history.releases,
           "history initializes once, retains valid contents, and initializes again after resize" );

    InitializingEngine imported;
    imported.lifetime = VRDG_IMPORTED;
    State importedState;
    Compile( &importedState, &imported );
    bool threw = false;
    try { Run( &importedState, &imported ); }
    catch ( const std::logic_error& ) { threw = true; }
    Check( threw && !imported.transitions && !imported.callbacks, "imported lifetime alone does not guarantee initial contents" );
    imported.importedValid = true;
    Run( &importedState, &imported );
    Check( imported.observed == std::vector< int >{ 77 } && !imported.creates && !imported.initializations && !imported.releases,
           "host supplies imported contents and retains ownership" );

    InitializingEngine failure;
    failure.initializeMissing = false;
    State failureState;
    Compile( &failureState, &failure );
    threw = false;
    try { Run( &failureState, &failure ); }
    catch ( const std::logic_error& ) { threw = true; }
    Check( threw && !failure.transitions && !failure.callbacks, "failed initialization prevents the affected task" );
    failure.initializeMissing = true;
    Run( &failureState, &failure );
    Check( failure.observed == std::vector< int >{ 0 }, "host can retry initialization without recompiling" );
    std::puts( "[PASS] Initial contents, discard, alias handoff, history, resize and imports" );
    return 0;
}
