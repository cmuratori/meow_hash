/* ========================================================================

   meow_bench.cpp - basic RDTSC-based benchmark for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __aarch64__
// NOTE(mmozeiko): On ARM you normally cannot access cycle counter from user-space.
// Download & build following kernel module that enables access to PMU cycle counter
// from user-space code: https://github.com/zhiyisun/enable_arm_pmu
#include <stdint.h>
#include "enable_arm_pmu/armpmu_lib.h"
#define __rdtsc() read_pmu()
#endif

#include "meow_test.h"

#define Kb(x) ((meow_u64)(x)*(meow_u64)1024)
#define Mb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024)
#define Gb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024*(meow_u64)1024)

enum input_size_type
{
    InputSizeType_Tiny0,
    InputSizeType_Tiny1,
    InputSizeType_Tiny2,
    InputSizeType_Tiny3,
    
    InputSizeType_Small,
    InputSizeType_Medium,
    InputSizeType_Large,
    InputSizeType_Giant,
    
    InputSizeType_Count,
};

struct test_results
{
    int unsigned HashType;
    
    meow_u64 Size;
    meow_u64 MinimumClocks;
    meow_u64 MedianClocks;
    
    // NOTE(casey): We follow Fabian Giesen's lead here and prefer the median performance
    // to the fastest, in the hopes that we will not consider a hash's performance to be
    // an unusual confluence of chip power distributions and branch predictions that give it
    // a few really good runs that aren't indicative of its typical performance.
    double MedianBPC;
};

struct input_size_test
{
    meow_u64 ClockCount;
    meow_u64 *Clocks;
    
    // NOTE(casey): We use a fake result target for writing in hopes of convincing the optimizer
    // that the computed hashes are actually used, and so won't be optimized out.
    meow_u128 FakeSlot;
    
    meow_u64 Size;
};

#define SIZE_COUNT_PER_BATCH 16
struct input_size_tests
{
    meow_u64 MaxClockCount;
    meow_u64 RunsPerHashImplementation;
    input_size_test Sizes[SIZE_COUNT_PER_BATCH];

    int unsigned ResultCount;
    test_results Results[SIZE_COUNT_PER_BATCH * InputSizeType_Count * ArrayCount(NamedHashTypes)];
    
    char *Name;
};

static void
FuddleBuffer(meow_u64 Size, void *Buffer, int Seed)
{
    meow_u128 *Dest = (meow_u128 *)Buffer;
    meow_u128 Stamp = Meow128_Set64x2(Seed, Seed + 1);
    for(meow_u64 Index = 0;
        Index < (Size >> 4);
        ++Index)
    {
        *Dest++ = Stamp;
    }
    
    meow_u8 *Overhang = (meow_u8 *)Dest;
    for(meow_u64 Index = 0;
        Index < (Size & 0xF);
        ++Index)
    {
        Overhang[Index] = 13*Index + Seed;
    }
}

static int
ResultCompare(const void *AInit, const void *BInit)
{
    test_results *A = (test_results *)AInit;
    test_results *B = (test_results *)BInit;
    
    int Result = 0;
    if(A->Size > B->Size)
    {
        Result = 1;
    }
    else if(A->Size < B->Size)
    {
        Result = -1;
    }
    else if(A->MedianClocks > B->MedianClocks)
    {
        Result = 1;
    }
    else if(A->MedianClocks < B->MedianClocks)
    {
        Result = -1;
    }
    
    return(Result);
}

static int
ClockCompare(const void *AInit, const void *BInit)
{
    meow_u64 A = *(meow_u64 *)AInit;
    meow_u64 B = *(meow_u64 *)BInit;
    
    int Result = 0;
    if(A > B)
    {
        Result = 1;
    }
    else if(B < A)
    {
        Result = -1;
    }
    
    return(Result);
}

static test_results *
ComputeStats(int unsigned HashType, input_size_test *Test, input_size_tests *DestTests)
{
    test_results *Results = 0;
    
    if(DestTests->ResultCount < ArrayCount(DestTests->Results))
    {
        Results = DestTests->Results + DestTests->ResultCount++;
        Results->HashType = HashType;
        Results->Size = Test->Size;
        
        if(Test->ClockCount)
        {
            qsort(Test->Clocks, Test->ClockCount, sizeof(Test->Clocks[0]), ClockCompare);
            Results->MinimumClocks = Test->Clocks[0];
            Results->MedianClocks = Test->Clocks[Test->ClockCount / 2];
            Results->MedianBPC = (double)Test->Size / (double)Results->MedianClocks;
        }
    }
    
    return(Results);
}

static void
InitializeTests(input_size_tests *Tests, input_size_type Type, meow_u64 MaxClockCount)
{
    int unsigned SmallOffset[] =
    {
        7,
        12,
        3,
        5,
        
        15,
        8,
        6,
        4,
        
        11,
        1,
        14,
        0,
        
        9,
        10,
        2,
        13,
    };
    
    int unsigned RegularOffset[] =
    {
        1,
        2,
        3,
        7,
        
        15,
        15 + 11,
        31,
        31 + 15,
        
        63,
        63 + 31,
        127,
        127 + 63,
        
        255,
        255 + 127,
        511,
        513,
    };
    
    char *Name = (char *)"(unnamed test)";
    meow_u64 Divisor = 1;
    for(int unsigned SizeIndex = 0;
        SizeIndex < ArrayCount(Tests->Sizes);
        ++SizeIndex)
    {
        meow_u64 Size = 0;
        switch(Type)
        {
            case InputSizeType_Tiny0:
            {
                Name = (char *)"Tiny Input (Pass 1)";
                Size = 16*SizeIndex + SmallOffset[(0 + SizeIndex) % ArrayCount(SmallOffset)];
            } break;
            
            case InputSizeType_Tiny1:
            {
                Name = (char *)"Tiny Input (Pass 2)";
                Size = 16*SizeIndex + SmallOffset[(4 + SizeIndex) % ArrayCount(SmallOffset)];
            } break;
            
            case InputSizeType_Tiny2:
            {
                Name = (char *)"Tiny Input (Pass 3)";
                Size = 16*SizeIndex + SmallOffset[(8 + SizeIndex) % ArrayCount(SmallOffset)];
            } break;
            
            case InputSizeType_Tiny3:
            {
                Name = (char *)"Tiny Input (Pass 4)";
                Size = 16*SizeIndex + SmallOffset[(12 + SizeIndex) % ArrayCount(SmallOffset)];
            } break;
            
            case InputSizeType_Small:
            {
                Name = (char *)"Small Input";
                Size = (Kb(SizeIndex + 1)) + RegularOffset[SizeIndex % ArrayCount(RegularOffset)];
                Divisor = 10;
            } break;
            
            case InputSizeType_Medium:
            {
                Name = (char *)"Medium Input";
                Size = (Kb(32 * (SizeIndex + 1))) + RegularOffset[SizeIndex % ArrayCount(RegularOffset)];
                Divisor = 200;
            } break;
            
            case InputSizeType_Large:
            {
                Name = (char *)"Large Input";
                Size = (Mb(SizeIndex + 1)) + RegularOffset[SizeIndex % ArrayCount(RegularOffset)];
                Divisor = 15000;
            } break;
            
            case InputSizeType_Giant:
            {
                Name = (char *)"Giant Input";
                Size = (Gb(SizeIndex + 1)) / 4 + RegularOffset[SizeIndex % ArrayCount(RegularOffset)];
                Divisor = 1000000;
            } break;
            
            default:
            {
                fprintf(stderr, "ERROR: Invalid test sizes type requested.\n");
            } break;
        }
        
        Tests->Sizes[SizeIndex].Size = Size;
    }

    Tests->Name = Name;
    Tests->MaxClockCount = (MaxClockCount / Divisor);
    Tests->RunsPerHashImplementation = (ArrayCount(Tests->Sizes) * Tests->MaxClockCount);
}

static void
PrintLeaderboard(input_size_tests *Tests, FILE *Stream)
{
    fprintf(Stream, "Leaderboard:\n");
    
    qsort(Tests->Results, Tests->ResultCount, sizeof(Tests->Results[0]), ResultCompare);
    
    int unsigned ResultIndex = 0;
    while(ResultIndex < Tests->ResultCount)
    {
        test_results *BestResults = Tests->Results + ResultIndex;
        
        meow_u64 CurSize = BestResults->Size;
        meow_u64 MedianClocks = BestResults->MedianClocks;
        meow_u64 MaxClocks = MedianClocks + (MedianClocks/ 100);
        
        fprintf(Stream, "    ");
        PrintSize(Stream, BestResults->Size, true);
        fprintf(Stream, ": %10.0f (%6.03f bytes/cycle) - ",
                (double)BestResults->MedianClocks, (double)BestResults->MedianBPC);
        
        int TieCount = 0;
        while(ResultIndex < Tests->ResultCount)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->Size == CurSize)
            {
                if(Results->MedianClocks <= MaxClocks)
                {
                    named_hash_type Type = NamedHashTypes[Results->HashType];
                    
                    if(TieCount)
                    {
                        fprintf(Stream, ", ");
                    }
                    
                    fprintf(Stream, "%s", Type.FullName);
                    
                    ++TieCount;
                }
                ++ResultIndex;
            }
            else
            {
                break;
            }
        }
        if(TieCount > 1)
        {
            fprintf(Stream, " (%d-way tie)", TieCount);
        }
        fprintf(Stream, "\n");
    }
}
        
int
main(int ArgCount, char **Args)
{
#if __aarch64__
    enable_pmu(0x008);
#endif
    
    InitializeHashesThatNeedInitializers();
    
    fprintf(stderr, "\n");
    fprintf(stderr, "meow_bench %s - basic RDTSC-based benchmark for the Meow hash\n", MEOW_HASH_VERSION_NAME);
    fprintf(stderr, "    See https://mollyrocket.com/meowhash for details\n");
    fprintf(stderr, "    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    fprintf(stderr, "             (You must turn it off in your OS if you haven't yet!)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Versions compiled into this benchmark:\n");
    
    meow_u64 MaxClockCount = (meow_u64)10000000;
    input_size_tests *Tests = (input_size_tests *)aligned_alloc(4, sizeof(input_size_tests));
    
    int unsigned TypeCount = ArrayCount(NamedHashTypes);
    for(int unsigned TypeIndex = 0;
        TypeIndex < TypeCount;
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stderr, "    %d. %s\n", TypeIndex + 1, Type.FullName);
    }
    
    for(int unsigned SizeIndex = 0;
        SizeIndex < ArrayCount(Tests->Sizes);
        ++SizeIndex)
    {
        Tests->Sizes[SizeIndex].Clocks = (meow_u64 *)aligned_alloc(4, MaxClockCount*sizeof(meow_u64));
    }
    
    fprintf(stderr, "\n");
    
    for(int unsigned SizeType = 0;
        SizeType < InputSizeType_Count;
        ++SizeType)
    {
        int unsigned TestCount = ArrayCount(Tests->Sizes);

        InitializeTests(Tests, (input_size_type)SizeType, MaxClockCount);
        fprintf(stderr, "\n----------------------------------------------------\n");
        fprintf(stderr, "\n%s\n", Tests->Name);
        fprintf(stderr, "\n----------------------------------------------------\n");
        
        // NOTE(casey): Allocate a buffer for input - if we can't get the biggest one we need, reduce the largest
        // test down to something we _can_ allocate
        void *Buffer = 0;
        while(TestCount)
        {
            Buffer = aligned_alloc(CACHE_LINE_ALIGNMENT, Tests->Sizes[TestCount - 1].Size);
            if(Buffer)
            {
                break;
            }
            else
            {
                --TestCount;
            }
        }
        
        //
        // NOTE(casey): Run the test sizes through each hash, "randomizing" the order to hopefully thwart the branch predictors as much as possible
        //
        
        if(Buffer)
        {
            meow_u64 RunsPerHashImplementation = Tests->RunsPerHashImplementation;
            meow_u64 TestRandSeed = (meow_u64)time(0);
            for(int unsigned TypeIndex = 0;
                TypeIndex < TypeCount;
                ++TypeIndex)
            {
                named_hash_type Type = NamedHashTypes[TypeIndex];
                fprintf(stderr, "\n%s:\n", Type.FullName);
                
                // NOTE(casey): Clear the clock count
                for(int unsigned SizeIndex = 0;
                    SizeIndex < ArrayCount(Tests->Sizes);
                    ++SizeIndex)
                {
                    Tests->Sizes[SizeIndex].ClockCount = 0;
                }
                
                TRY
                {
                    meow_u64 ClocksSinceLastStatus = 0;
                    meow_u64 TestRand = TestRandSeed;
                    for(int unsigned RunIndex = 0;
                         RunIndex < Tests->RunsPerHashImplementation;
                         ++RunIndex)
                    {
                        // NOTE(casey): This is a XorShift64* LCG followed by an
                        // O'Neill random rotation
                        TestRand ^= TestRand >> 12;
                        TestRand ^= TestRand << 25;
                        TestRand ^= TestRand >> 27;
                        int unsigned UseIndex = _rotl((int unsigned)((TestRand ^ (TestRand>>18))>>27), (int)(TestRand >> 59));
                        TestRand = TestRand * 2685821657736338717LL;
                        
                        input_size_test *Test = Tests->Sizes + (UseIndex % TestCount);
                        
                        meow_u64 Size = Test->Size;
                        
                        // NOTE(casey): Write junk into the buffer to try to thwart the optimizer from removing the actual function call.
                        // This should also warm the cache so that small inputs will be read from cache instead of from memory.
                        FuddleBuffer(Size, Buffer, RunIndex);
                        
                        int Ignored[4];
                        int unsigned Ignored2;
                        __cpuid(Ignored, 0);
                        meow_u64 StartClock = __rdtsc();
                        Test->FakeSlot = Type.Imp(0, Size, Buffer);
                        meow_u64 EndClock = __rdtscp(&Ignored2);
                        __cpuid(Ignored, 0);
                        
                        meow_u64 Clocks = EndClock - StartClock;
                        if(Test->ClockCount < Tests->MaxClockCount)
                        {
                            Test->Clocks[Test->ClockCount++] = Clocks;
                        }
                        
                        ClocksSinceLastStatus += Clocks;
                        if((RunIndex == (RunsPerHashImplementation - 1)) ||
                           (ClocksSinceLastStatus > 1000000000ULL))
                        {
                            ClocksSinceLastStatus = 0;
                            fprintf(stderr, "\r    Test %0.0f / %0.0f (%0.0f%%)",
                                    (double)(RunIndex + 1), (double)RunsPerHashImplementation,
                                    100.0f * (double)(RunIndex + 1) / (double)RunsPerHashImplementation);
                        }
                    }
                    fprintf(stderr, "\n");
                    
                    for(int TestIndex = 0;
                        TestIndex < TestCount;
                        ++TestIndex)
                    {
                        input_size_test *Test = Tests->Sizes + TestIndex;
                        test_results *Results = ComputeStats(TypeIndex, Test, Tests);
                        if(Results)
                        {
                            fprintf(stderr, "    ");
                            PrintSize(stderr, Test->Size, true);
                            
                            fprintf(stderr, ": %0.03f bytes/cycle (%0.0f min, %0.0f med in %0.0f samples)\n",
                                    (double)Results->MedianBPC, (double)Results->MinimumClocks, (double)Results->MedianClocks,
                                    (double)Test->ClockCount);
                        }
                        else
                        {
                            fprintf(stderr, "ERROR: Out of result space!\n");
                        }
                    }
                    
                    //
                    // NOTE(casey): Print the incremental leaderboard
                    //
                    PrintLeaderboard(Tests, stderr);
                    fflush(stderr);
                }
                CATCH
                {
                    fprintf(stderr, "    (not supported on this CPU)\n");
                }
            }
            
            free(Buffer);
        }
        
        fprintf(stderr, "\n");
        
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    
    //
    // NOTE(casey): Print the final leaderboard
    //
    PrintLeaderboard(Tests, stderr);
                                  
    //
    // NOTE(casey): Print a CSV-style section for peeps who want to graph
    //
    
    meow_u64 LastSize = 0;
    fprintf(stdout, "Input");
    for(int ResultIndex = 0;
        ResultIndex < Tests->ResultCount;
        ++ResultIndex)
    {
        test_results *Results = Tests->Results + ResultIndex;
        if(Results->Size != LastSize)
        {
            fprintf(stdout, ",");
            LastSize = Results->Size;
            PrintSize(stdout, LastSize, false);
        }
    }
    fprintf(stdout, "\n");
    
    for(int TypeIndex = 0;
        TypeIndex < TypeCount;
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stdout, "%s", Type.FullName);
        for(int ResultIndex = 0;
            ResultIndex < Tests->ResultCount;
            ++ResultIndex)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->HashType == TypeIndex)
            {
                fprintf(stdout, ",%f", Results->MedianBPC);
            }
        }
        fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    fflush(stdout);
    
#if __aarch64__
    disable_pmu(0x008);
#endif
    
    return(0);
}
