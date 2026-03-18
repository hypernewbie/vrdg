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

#include <cstdio>
#include <chrono>
#include <cstdint>
#include "vrdg.h"
#include "test.h"

// ============================================ PHASE 1: INITIALIZATION ============================================
// Create initial resources: 20 buffers and 15 framebuffers with varying sizes
///> Graphics_Task( Init )
///>     Create_Buffer( buf_a, (uint32_t)4, (uint32_t)0, (uint32_t)0 )
///>     Create_Buffer( buf_b, 8, 0, 0 )
///>     Create_Buffer( buf_c, 16, 0, 0 )
///>     Create_Buffer( buf_d, 32, 0, 0 )
///>     Create_Buffer( buf_e, 64, 0, 0 )
///>     Create_Buffer( buf_f, 128, 0, 0 )
///>     Create_Buffer( buf_g, 256, 0, 0 )
///>     Create_Buffer( buf_h, 512, 0, 0 )
///>     Create_Buffer( buf_i, 1024, 0, 0 )
///>     Create_Buffer( buf_j, 2048, 0, 0 )
///>     Create_Buffer( buf_k, 4, 0, 0 )
///>     Create_Buffer( buf_l, 8, 0, 0 )
///>     Create_Buffer( buf_m, 16, 0, 0 )
///>     Create_Buffer( buf_n, 32, 0, 0 )
///>     Create_Buffer( buf_o, 64, 0, 0 )
///>     Create_TextureFB( fb_p, (uint32_t)16, (uint32_t)16, (uint32_t)0, (uint32_t)1, 0 )
///>     Create_TextureFB( fb_q, 32, 32, 0, 1, 0 )
///>     Create_TextureFB( fb_r, 64, 64, 0, 1, 0 )
///>     Create_TextureFB( fb_s, 128, 128, 0, 2, 0 )
///>     Create_TextureFB( fb_t, 256, 256, 0, 2, 0 )
///>     Create_TextureFB( fb_u, 16, 16, 0, 1, 0 )
///>     Create_TextureFB( fb_v, 32, 32, 0, 1, 0 )
///>     Create_TextureFB( fb_w, 64, 64, 0, 1, 0 )
///>     Create_TextureFB( fb_x, 128, 128, 0, 2, 0 )
///>     Create_TextureFB( fb_y, 256, 256, 0, 2, 0 )
///> End_Task

// ============================================ PHASE 2: DEEP CHAIN (10 levels) ============================================
// A->B->C->D->E->F->G->H->I->J linear dependency chain

///> Graphics_Task( Chain_A )
///>     Write_Buffer( buf_a )
///>     CPU_Profile( Chain_A )
///>     Func( ChainA_Func )
///> End_Task

///> Graphics_Task( Chain_B )
///>     Read_Buffer( buf_a )
///>     Write_Buffer( buf_b )
///>     CPU_Profile( Chain_B )
///>     Func( ChainB_Func )
///> End_Task

///> Graphics_Task( Chain_C )
///>     Read_Buffer( buf_b )
///>     Write_Buffer( buf_c )
///>     CPU_Profile( Chain_C )
///>     Func( ChainC_Func )
///> End_Task

///> Graphics_Task( Chain_D )
///>     Read_Buffer( buf_c )
///>     Write_Buffer( buf_d )
///>     CPU_Profile( Chain_D )
///>     Func( ChainD_Func )
///> End_Task

///> Graphics_Task( Chain_E )
///>     Read_Buffer( buf_d )
///>     Write_Buffer( buf_e )
///>     CPU_Profile( Chain_E )
///>     Func( ChainE_Func )
///> End_Task

///> Graphics_Task( Chain_F )
///>     Read_Buffer( buf_e )
///>     Write_Buffer( buf_f )
///>     CPU_Profile( Chain_F )
///>     Func( ChainF_Func )
///> End_Task

///> Graphics_Task( Chain_G )
///>     Read_Buffer( buf_f )
///>     Write_Buffer( buf_g )
///>     CPU_Profile( Chain_G )
///>     Func( ChainG_Func )
///> End_Task

///> Graphics_Task( Chain_H )
///>     Read_Buffer( buf_g )
///>     Write_Buffer( buf_h )
///>     CPU_Profile( Chain_H )
///>     Func( ChainH_Func )
///> End_Task

///> Graphics_Task( Chain_I )
///>     Read_Buffer( buf_h )
///>     Write_Buffer( buf_i )
///>     CPU_Profile( Chain_I )
///>     Func( ChainI_Func )
///> End_Task

///> Graphics_Task( Chain_J )
///>     Read_Buffer( buf_i )
///>     Write_Buffer( buf_j )
///>     CPU_Profile( Chain_J )
///>     Func( ChainJ_Func )
///> End_Task

// ============================================ PHASE 3: DIAMOND DEPENDENCIES ============================================
// buf_j is read by 4 parallel tasks (Diamond_1-4)

///> Graphics_Task( Diamond_1 )
///>     Read_Buffer( buf_j )
///>     Write_Buffer( buf_k )
///>     CPU_Profile( Diamond_1 )
///>     Func( Diamond1_Func )
///> End_Task

///> Graphics_Task( Diamond_2 )
///>     Read_Buffer( buf_j )
///>     Write_Buffer( buf_l )
///>     CPU_Profile( Diamond_2 )
///>     Func( Diamond2_Func )
///> End_Task

///> Graphics_Task( Diamond_3 )
///>     Read_Buffer( buf_j )
///>     Write_TextureFB( fb_p )
///>     CPU_Profile( Diamond_3 )
///>     Func( Diamond3_Func )
///> End_Task

///> Graphics_Task( Diamond_4 )
///>     Read_Buffer( buf_j )
///>     Write_TextureFB( fb_q )
///>     CPU_Profile( Diamond_4 )
///>     Func( Diamond4_Func )
///> End_Task

// Merge point: 4 resources into 1 task
///> Graphics_Task( Diamond_Merge )
///>     Read_Buffer( buf_k )
///>     Read_Buffer( buf_l )
///>     Read_TextureFB( fb_p )
///>     Read_TextureFB( fb_q )
///>     Write_Buffer( buf_m )
///>     CPU_Profile( Diamond_Merge )
///>     Func( DiamondMerge_Func )
///> End_Task

// ============================================ PHASE 4: COMPUTE TASKS ============================================
// Mix compute and graphics tasks

///> Compute_Task( Compute_1 )
///>     Read_Buffer( buf_m )
///>     Write_TextureFB( fb_r )
///>     CPU_Profile( Compute_1 )
///>     Func( Compute1_Func )
///> End_Task

///> Compute_Task( Compute_2 )
///>     Read_TextureFB( fb_r )
///>     Write_TextureFB( fb_s )
///>     CPU_Profile( Compute_2 )
///>     Func( Compute2_Func )
///> End_Task

///> Graphics_Task( Graphics_1 )
///>     Read_TextureFB( fb_s )
///>     Write_TextureFB( fb_t )
///>     CPU_Profile( Graphics_1 )
///>     Func( Graphics1_Func )
///> End_Task

// ============================================ PHASE 5: MULTIPLE INDEPENDENT CHAINS ============================================
// Two parallel independent chains for stress testing

///> Graphics_Task( Parallel_A1 )
///>     Write_Buffer( buf_n )
///>     CPU_Profile( Parallel_A1 )
///>     Func( ParallelA1_Func )
///> End_Task

///> Graphics_Task( Parallel_A2 )
///>     Read_Buffer( buf_n )
///>     Write_TextureFB( fb_u )
///>     CPU_Profile( Parallel_A2 )
///>     Func( ParallelA2_Func )
///> End_Task

///> Graphics_Task( Parallel_A3 )
///>     Read_TextureFB( fb_u )
///>     Write_TextureFB( fb_v )
///>     CPU_Profile( Parallel_A3 )
///>     Func( ParallelA3_Func )
///> End_Task

///> Graphics_Task( Parallel_B1 )
///>     Write_Buffer( buf_o )
///>     CPU_Profile( Parallel_B1 )
///>     Func( ParallelB1_Func )
///> End_Task

///> Graphics_Task( Parallel_B2 )
///>     Read_Buffer( buf_o )
///>     Write_TextureFB( fb_w )
///>     CPU_Profile( Parallel_B2 )
///>     Func( ParallelB2_Func )
///> End_Task

///> Graphics_Task( Parallel_B3 )
///>     Read_TextureFB( fb_w )
///>     Write_TextureFB( fb_x )
///>     CPU_Profile( Parallel_B3 )
///>     Func( ParallelB3_Func )
///> End_Task

// ============================================ PHASE 6: COMPLEX MERGE ============================================
// Merge parallel chains

///> Graphics_Task( Parallel_Merge )
///>     Read_TextureFB( fb_v )
///>     Read_TextureFB( fb_x )
///>     Read_TextureFB( fb_t )
///>     Write_TextureFB( fb_y )
///>     CPU_Profile( Parallel_Merge )
///>     Func( ParallelMerge_Func )
///> End_Task

// ============================================ PHASE 7: RESOURCE LIFECYCLE EDGE CASES ============================================
// Tasks that only read (no writes) - verify they don't need release timing adjustment

///> Graphics_Task( ReadOnly_1 )
///>     Read_TextureFB( fb_y )
///>     CPU_Profile( ReadOnly_1 )
///>     Func( ReadOnly1_Func )
///> End_Task

///> Graphics_Task( ReadOnly_2 )
///>     Read_TextureFB( fb_y )
///>     CPU_Profile( ReadOnly_2 )
///>     Func( ReadOnly2_Func )
///> End_Task

// ============================================ PHASE 8: GPU PROFILING ============================================
// Test GPU_Profile annotation generates vrhi timer queries

///> Graphics_Task( GpuProfile_Test )
///>     Write_Buffer( buf_a )
///>     GPU_Profile( GpuProfilePass )
///>     CPU_Profile( GpuProfile_Test )
///>     Func( GpuProfileTest_Func )
///> End_Task

///> Graphics_Task( GpuProfile_Test2 )
///>     Read_Buffer( buf_a )
///>     GPU_Profile( GpuProfilePass2 )
///>     Func( GpuProfileTest2_Func )
///> End_Task

// ============================================ FUNCTION IMPLEMENTATIONS ============================================

namespace vrdg {

// Deep chain functions
void ChainA_Func( VRDG_BUFFER buf_a ) {
    printf( "    Chain_A : buf_a = %llu\n", (unsigned long long)buf_a );
}

void ChainB_Func( VRDG_BUFFER buf_a, VRDG_BUFFER buf_b ) {
    printf( "    Chain_B : buf_a = %llu, buf_b = %llu\n", (unsigned long long)buf_a, (unsigned long long)buf_b );
}

void ChainC_Func( VRDG_BUFFER buf_b, VRDG_BUFFER buf_c ) {
    printf( "    Chain_C : buf_b = %llu, buf_c = %llu\n", (unsigned long long)buf_b, (unsigned long long)buf_c );
}

void ChainD_Func( VRDG_BUFFER buf_c, VRDG_BUFFER buf_d ) {
    printf( "    Chain_D : buf_c = %llu, buf_d = %llu\n", (unsigned long long)buf_c, (unsigned long long)buf_d );
}

void ChainE_Func( VRDG_BUFFER buf_d, VRDG_BUFFER buf_e ) {
    printf( "    Chain_E : buf_d = %llu, buf_e = %llu\n", (unsigned long long)buf_d, (unsigned long long)buf_e );
}

void ChainF_Func( VRDG_BUFFER buf_e, VRDG_BUFFER buf_f ) {
    printf( "    Chain_F : buf_e = %llu, buf_f = %llu\n", (unsigned long long)buf_e, (unsigned long long)buf_f );
}

void ChainG_Func( VRDG_BUFFER buf_f, VRDG_BUFFER buf_g ) {
    printf( "    Chain_G : buf_f = %llu, buf_g = %llu\n", (unsigned long long)buf_f, (unsigned long long)buf_g );
}

void ChainH_Func( VRDG_BUFFER buf_g, VRDG_BUFFER buf_h ) {
    printf( "    Chain_H : buf_g = %llu, buf_h = %llu\n", (unsigned long long)buf_g, (unsigned long long)buf_h );
}

void ChainI_Func( VRDG_BUFFER buf_h, VRDG_BUFFER buf_i ) {
    printf( "    Chain_I : buf_h = %llu, buf_i = %llu\n", (unsigned long long)buf_h, (unsigned long long)buf_i );
}

void ChainJ_Func( VRDG_BUFFER buf_i, VRDG_BUFFER buf_j ) {
    printf( "    Chain_J : buf_i = %llu, buf_j = %llu\n", (unsigned long long)buf_i, (unsigned long long)buf_j );
}

// Diamond functions
void Diamond1_Func( VRDG_BUFFER buf_j, VRDG_BUFFER buf_k ) {
    printf( "    Diamond_1 : buf_j = %llu, buf_k = %llu\n", (unsigned long long)buf_j, (unsigned long long)buf_k );
}

void Diamond2_Func( VRDG_BUFFER buf_j, VRDG_BUFFER buf_l ) {
    printf( "    Diamond_2 : buf_j = %llu, buf_l = %llu\n", (unsigned long long)buf_j, (unsigned long long)buf_l );
}

void Diamond3_Func( VRDG_BUFFER buf_j, VRDG_FRAMEBUFFER fb_p ) {
    printf( "    Diamond_3 : buf_j = %llu, fb_p = %llu\n", (unsigned long long)buf_j, (unsigned long long)fb_p );
}

void Diamond4_Func( VRDG_BUFFER buf_j, VRDG_FRAMEBUFFER fb_q ) {
    printf( "    Diamond_4 : buf_j = %llu, fb_q = %llu\n", (unsigned long long)buf_j, (unsigned long long)fb_q );
}

void DiamondMerge_Func( VRDG_BUFFER buf_k, VRDG_BUFFER buf_l, VRDG_FRAMEBUFFER fb_p, VRDG_FRAMEBUFFER fb_q, VRDG_BUFFER buf_m ) {
    printf( "    Diamond_Merge : buf_k = %llu, buf_l = %llu, fb_p = %llu, fb_q = %llu, buf_m = %llu\n", 
            (unsigned long long)buf_k, (unsigned long long)buf_l, (unsigned long long)fb_p, (unsigned long long)fb_q, (unsigned long long)buf_m );
}

// Compute functions
void Compute1_Func( VRDG_BUFFER buf_m, VRDG_FRAMEBUFFER fb_r ) {
    printf( "    Compute_1 : buf_m = %llu, fb_r = %llu\n", (unsigned long long)buf_m, (unsigned long long)fb_r );
}

void Compute2_Func( VRDG_FRAMEBUFFER fb_r, VRDG_FRAMEBUFFER fb_s ) {
    printf( "    Compute_2 : fb_r = %llu, fb_s = %llu\n", (unsigned long long)fb_r, (unsigned long long)fb_s );
}

void Graphics1_Func( VRDG_FRAMEBUFFER fb_s, VRDG_FRAMEBUFFER fb_t ) {
    printf( "    Graphics_1 : fb_s = %llu, fb_t = %llu\n", (unsigned long long)fb_s, (unsigned long long)fb_t );
}

// Parallel chain A functions
void ParallelA1_Func( VRDG_BUFFER buf_n ) {
    printf( "    Parallel_A1 : buf_n = %llu\n", (unsigned long long)buf_n );
}

void ParallelA2_Func( VRDG_BUFFER buf_n, VRDG_FRAMEBUFFER fb_u ) {
    printf( "    Parallel_A2 : buf_n = %llu, fb_u = %llu\n", (unsigned long long)buf_n, (unsigned long long)fb_u );
}

void ParallelA3_Func( VRDG_FRAMEBUFFER fb_u, VRDG_FRAMEBUFFER fb_v ) {
    printf( "    Parallel_A3 : fb_u = %llu, fb_v = %llu\n", (unsigned long long)fb_u, (unsigned long long)fb_v );
}

// Parallel chain B functions
void ParallelB1_Func( VRDG_BUFFER buf_o ) {
    printf( "    Parallel_B1 : buf_o = %llu\n", (unsigned long long)buf_o );
}

void ParallelB2_Func( VRDG_BUFFER buf_o, VRDG_FRAMEBUFFER fb_w ) {
    printf( "    Parallel_B2 : buf_o = %llu, fb_w = %llu\n", (unsigned long long)buf_o, (unsigned long long)fb_w );
}

void ParallelB3_Func( VRDG_FRAMEBUFFER fb_w, VRDG_FRAMEBUFFER fb_x ) {
    printf( "    Parallel_B3 : fb_w = %llu, fb_x = %llu\n", (unsigned long long)fb_w, (unsigned long long)fb_x );
}

// Merge function
void ParallelMerge_Func( VRDG_FRAMEBUFFER fb_v, VRDG_FRAMEBUFFER fb_x, VRDG_FRAMEBUFFER fb_t, VRDG_FRAMEBUFFER fb_y ) {
    printf( "    Parallel_Merge : fb_v = %llu, fb_x = %llu, fb_t = %llu, fb_y = %llu\n", 
            (unsigned long long)fb_v, (unsigned long long)fb_x, (unsigned long long)fb_t, (unsigned long long)fb_y );
}

// Read-only functions
void ReadOnly1_Func( VRDG_FRAMEBUFFER fb_y ) {
    printf( "    ReadOnly_1 : fb_y = %llu\n", (unsigned long long)fb_y );
}

void ReadOnly2_Func( VRDG_FRAMEBUFFER fb_y ) {
    printf( "    ReadOnly_2 : fb_y = %llu\n", (unsigned long long)fb_y );
}

// GPU Profile functions
void GpuProfileTest_Func( VRDG_BUFFER buf_a ) {
    printf( "    GpuProfile_Test : buf_a = %llu\n", (unsigned long long)buf_a );
}

void GpuProfileTest2_Func( VRDG_BUFFER buf_a ) {
    printf( "    GpuProfile_Test2 : buf_a = %llu\n", (unsigned long long)buf_a );
}

} // namespace vrdg

// ============================================ TEST ENGINE INTERFACE ============================================

class ExtendedEngineInterfaceTest : public vrdg::EngineInterface
{
private:
    int creationCount = 0;
    int releaseCount = 0;
    int transitionCount = 0;

public:
    virtual void CreateBuffer( vrdg::Buffer idx, const std::string& name, uint64_t sz, uint32_t format, uint32_t flags ) override
    {
        creationCount++;
        printf( "  [CREATE] Buffer %s (id=%llu, size=%llu bytes)\n", name.c_str(), (unsigned long long)idx, sz );
    }

    virtual void ReleaseBuffer( vrdg::Buffer idx ) override
    {
        releaseCount++;
        printf( "  [RELEASE] Buffer (id=%llu)\n", (unsigned long long)idx );
    }

    virtual VRDG_BUFFER GetEngineBufferObject( vrdg::Buffer idx ) override
    {
        return (VRDG_BUFFER)(0x1000 + idx);
    }

    virtual void TransitionBuffer( vrdg::Buffer idx, uint64_t flags ) override
    {
        transitionCount++;
        const char* flagStr = (flags & vrdg::VRDG_READ) ? "READ" : (flags & vrdg::VRDG_WRITE) ? "WRITE" : "UNKNOWN";
        const char* graphicsStr = (flags & vrdg::VRDG_GRAPHICS) ? "GRAPHICS" : (flags & vrdg::VRDG_COMPUTE) ? "COMPUTE" : "";
        printf( "  [TRANSITION] Buffer (id=%llu) %s %s\n", (unsigned long long)idx, flagStr, graphicsStr );
    }

    virtual void CreateFB( vrdg::FrameBuffer idx, const std::string& name, uint32_t width, uint32_t height, uint32_t format, uint32_t mips, uint64_t flags = 0 ) override
    {
        creationCount++;
        printf( "  [CREATE] FrameBuffer %s (id=%llu, %ux%u, mips=%u)\n", name.c_str(), (unsigned long long)idx, width, height, mips );
    }

    virtual void ReleaseFB( vrdg::FrameBuffer idx ) override
    {
        releaseCount++;
        printf( "  [RELEASE] FrameBuffer (id=%llu)\n", (unsigned long long)idx );
    }

    virtual VRDG_FRAMEBUFFER GetEngineFBObject( vrdg::FrameBuffer idx ) override
    {
        return (VRDG_FRAMEBUFFER)(0x2000 + idx);
    }

    virtual void TransitionFB( vrdg::FrameBuffer idx, uint64_t flags ) override
    {
        transitionCount++;
        const char* flagStr = (flags & vrdg::VRDG_READ) ? "READ" : (flags & vrdg::VRDG_WRITE) ? "WRITE" : "UNKNOWN";
        const char* graphicsStr = (flags & vrdg::VRDG_GRAPHICS) ? "GRAPHICS" : (flags & vrdg::VRDG_COMPUTE) ? "COMPUTE" : "";
        printf( "  [TRANSITION] FrameBuffer (id=%llu) %s %s\n", (unsigned long long)idx, flagStr, graphicsStr );
    }

    void PrintStats()
    {
        printf( "\n[STATISTICS]\n" );
        printf( "  Total Creations: %d\n", creationCount );
        printf( "  Total Releases: %d\n", releaseCount );
        printf( "  Total Transitions: %d\n", transitionCount );
    }
};

// ============================================ BUILD IMPLEMENTATION ============================================

class ComplexEngineInterfaceTest : public ExtendedEngineInterfaceTest
{
public:
    virtual void Build( vrdg::State* s ) override
    {
        using namespace vrdg;

        Buffer buf_a, buf_b, buf_c, buf_d, buf_e, buf_f, buf_g, buf_h, buf_i, buf_j;
        Buffer buf_k, buf_l, buf_m, buf_n, buf_o;
        FrameBuffer fb_p, fb_q, fb_r, fb_s, fb_t, fb_u, fb_v, fb_w, fb_x, fb_y;

        // Phase 1: Initialize all resources
        Init( s, buf_a, buf_b, buf_c, buf_d, buf_e, buf_f, buf_g, buf_h, buf_i, buf_j,
              buf_k, buf_l, buf_m, buf_n, buf_o,
              fb_p, fb_q, fb_r, fb_s, fb_t, fb_u, fb_v, fb_w, fb_x, fb_y );

        // Phase 2: Deep chain (10 levels)
        Chain_A( s, buf_a );
        Chain_B( s, buf_a, buf_b );
        Chain_C( s, buf_b, buf_c );
        Chain_D( s, buf_c, buf_d );
        Chain_E( s, buf_d, buf_e );
        Chain_F( s, buf_e, buf_f );
        Chain_G( s, buf_f, buf_g );
        Chain_H( s, buf_g, buf_h );
        Chain_I( s, buf_h, buf_i );
        Chain_J( s, buf_i, buf_j );

        // Phase 3: Diamond dependencies
        Diamond_1( s, buf_j, buf_k );
        Diamond_2( s, buf_j, buf_l );
        Diamond_3( s, buf_j, fb_p );
        Diamond_4( s, buf_j, fb_q );
        Diamond_Merge( s, buf_k, buf_l, fb_p, fb_q, buf_m );

        // Phase 4: Compute tasks mixed with graphics
        Compute_1( s, buf_m, fb_r );
        Compute_2( s, fb_r, fb_s );
        Graphics_1( s, fb_s, fb_t );

        // Phase 5: Parallel independent chains
        Parallel_A1( s, buf_n );
        Parallel_A2( s, buf_n, fb_u );
        Parallel_A3( s, fb_u, fb_v );

        Parallel_B1( s, buf_o );
        Parallel_B2( s, buf_o, fb_w );
        Parallel_B3( s, fb_w, fb_x );

        // Phase 6: Complex merge
        Parallel_Merge( s, fb_v, fb_x, fb_t, fb_y );

        // Phase 7: Read-only tasks
        ReadOnly_1( s, fb_y );
        ReadOnly_2( s, fb_y );

        // Phase 8: GPU profiling tasks
        GpuProfile_Test( s, buf_a );
        GpuProfile_Test2( s, buf_a );
    }
};

// ============================================ HASH RECOMPILE TEST ENGINE ============================================

class HashRecompileEngineTest : public ComplexEngineInterfaceTest
{
public:
    int configVersion = 0;
    int buildCount = 0;

    virtual void HashGraphConfigOptions( uint64_t& hash0, uint64_t& hash1 ) override
    {
        // Simple hash based on config version
        hash0 = 1315423911;
        hash1 = 13;
        hash0 ^= ( ( hash0 << 5 ) + configVersion + ( hash0 >> 2 ) );
        hash1 ^= ( ( hash1 << 5 ) + configVersion + ( hash1 >> 2 ) );
    }

    virtual void Build( vrdg::State* s ) override
    {
        buildCount++;
        printf( "  [HASH] Build #%d (config=%d)\n", buildCount, configVersion );
        ComplexEngineInterfaceTest::Build( s );
    }
};

// ============================================ MAIN TEST ============================================

vrdg::State state;
ComplexEngineInterfaceTest test;

int main()
{
    printf( "VRDG Extensive Test Suite\n" );
    printf( "=========================\n\n" );

    auto start = std::chrono::high_resolution_clock::now();

    printf( "[COMPILE 1 - Initial graph build]\n" );
    vrdg::Compile( &state, &test );
    printf( "  Tasks registered: %zu\n\n", state.tasks.size() );

    printf( "[RUN 1 - First execution]\n" );
    vrdg::Run( &state, &test );

    printf( "\n[RUN 2 - Second execution (should reuse compiled graph)]\n" );
    vrdg::Run( &state, &test );

    printf( "\n[RUN 3 - Third execution]\n" );
    vrdg::Run( &state, &test );

    printf( "\n[COMPILE 2 - Recompile with same config]\n" );
    vrdg::Compile( &state, &test );
    printf( "  Tasks registered: %zu (should be same as before)\n\n", state.tasks.size() );

    printf( "[RUN 4 - Execution after recompile]\n" );
    vrdg::Run( &state, &test );

    // ============================================ HASH RECOMPILE TESTS ============================================
    printf( "\n[HASH RECOMPILE TESTS]\n" );
    
    vrdg::State hashState;
    HashRecompileEngineTest hashTest;
    
    printf( "\n[HASH TEST] Compile A (config=0)\n" );
    hashTest.configVersion = 0;
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d, tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );
    
    printf( "\n[HASH TEST] Compile A again (same config)\n" );
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d (should be 1), tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );
    
    printf( "\n[HASH TEST] Compile B (config changed to 1)\n" );
    hashTest.configVersion = 1;
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d (should be 2), tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );
    
    printf( "\n[HASH TEST] Compile B again (same config)\n" );
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d (should be 2), tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );
    
    printf( "\n[HASH TEST] Compile C (config changed to 2)\n" );
    hashTest.configVersion = 2;
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d (should be 3), tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );
    
    printf( "\n[HASH TEST] Back to A (config changed to 0)\n" );
    hashTest.configVersion = 0;
    vrdg::Compile( &hashState, &hashTest );
    printf( "  [HASH] buildCount=%d (should be 4), tasks=%zu\n", hashTest.buildCount, hashState.tasks.size() );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    printf( "\n" );
    static_cast<ExtendedEngineInterfaceTest&>(test).PrintStats();
    printf( "  Total execution time: %lld ms\n", (long long)duration.count() );

    printf( "\n[TEST COMPLETE]\n" );
    return 0;
}
