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
#define __rdtscp(x) read_pmu()
#endif

#ifndef _WIN32
static int unsigned
_rotl(int unsigned Value, int Count)
{
    int unsigned Result = (Value << Count) | (Value >> (32 - Count));
    return Result;
}
#endif

#ifndef _MSC_VER

#if __i386__
#define __cpuid(Array, Value) __asm__ __volatile__("cpuid" : : : "eax", "ebx", "ecx", "edx")
#elif __x86_64__
#define __cpuid(Array, Value) __asm__ __volatile__("cpuid" : : : "rax", "rbx", "rcx", "rdx")
#else
#define __cpuid(Array, Value)
#endif

#endif

#include "meow_test.h"

#define Kb(x) ((meow_u64)(x)*(meow_u64)1024)
#define Mb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024)
#define Gb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024*(meow_u64)1024)

struct test_results
{
    int unsigned HashType;
    
    meow_u64 Size;
    meow_u64 MinClocks;
    meow_u64 ExpClocks;
    
    double MinBPC;
    double ExpBPC;
};

struct input_size_test
{
    meow_u64 ClockCount;
    meow_u64 ClockAccum;
    meow_u64 ClockExp;
    meow_u64 ClockMin;
    
    // NOTE(casey): We use a fake result target for writing in hopes of convincing the optimizer
    // that the computed hashes are actually used, and so won't be optimized out.
    meow_u128 FakeSlot;
    
    meow_u64 Size;
};

#define MAX_SIZE_TO_TEST Gb(2)
#define SIZE_TYPE_COUNT 64
#define SIZE_COUNT_PER_BATCH 16
struct input_size_tests
{
    meow_u64 MaxClockCount;
    meow_u64 RunsPerHashImplementation;
    input_size_test Sizes[SIZE_COUNT_PER_BATCH];

    int unsigned ResultCount;
    test_results Results[SIZE_COUNT_PER_BATCH * SIZE_TYPE_COUNT * ArrayCount(NamedHashTypes)];
    
    meow_u64 SizeSeries;
    char Name[64];
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
    else if(A->ExpClocks > B->ExpClocks)
    {
        Result = 1;
    }
    else if(A->ExpClocks < B->ExpClocks)
    {
        Result = -1;
    }
    
    return(Result);
}

static test_results *
CommitResults(int unsigned HashType, input_size_test *Test, input_size_tests *DestTests)
{
    test_results *Results = 0;
    
    if(DestTests->ResultCount < ArrayCount(DestTests->Results))
    {
        Results = DestTests->Results + DestTests->ResultCount++;
        Results->HashType = HashType;
        Results->Size = Test->Size;
        Results->MinClocks = Test->ClockMin;
        Results->ExpClocks = Test->ClockExp;
        
        Results->ExpBPC = 0.0f;
        if(Results->ExpClocks)
        {
            Results->ExpBPC = (double)Test->Size / (double)Results->ExpClocks;
        }
        
        Results->MinBPC = 0.0f;
        if(Results->MinClocks)
        {
            Results->MinBPC = (double)Test->Size / (double)Results->MinClocks;
        }
    }
    
    return(Results);
}

static int unsigned
Random(meow_u64 *Series)
{
    meow_u64 TestRand = *Series;
    
    // NOTE(casey): This is a XorShift64* (LCG) followed by an O'Neill random rotation (PCG)
    TestRand ^= TestRand >> 12;
    TestRand ^= TestRand << 25;
    TestRand ^= TestRand >> 27;
    
    int unsigned Result = _rotl((int unsigned)((TestRand ^ (TestRand>>18))>>27), (int)(TestRand >> 59));
    *Series = TestRand * 2685821657736338717LL;
    
    return(Result);
}

static void
InitializeTests(input_size_tests *Tests, int unsigned SizeType, meow_u64 MaxClockCount)
{
    char *NameBase = (char *)"(unknown)";
    meow_u64 Start = 1;
    meow_u64 End = 1024;
    meow_u64 Divisor = 1;
    if(SizeType < 24)
    {
        // NOTE(casey): 1b - 1024b
        NameBase = (char *)"Tiny Input";
        
        Start = 1;
        End = 1024;
    }
    else if(SizeType < 44)
    {
        // NOTE(casey): 1k - 64k
        NameBase = (char *)"Small Input";
        Divisor = 2;
        
        Start = Kb(1);
        End = Kb(64);
    }
    else if(SizeType < 58)
    {
        // NOTE(casey): 64k - 1mb
        NameBase = (char *)"Medium Input";
        Divisor = 10;
        
        Start = Kb(64);
        End = Mb(1);
    }
    else if(SizeType < 62)
    {
        // NOTE(casey): 1mb - 512mb
        NameBase = (char *)"Large Input";
        Divisor = 100;
        
        Start = Mb(1);
        End = Mb(512);
    }
    else if(SizeType < 64)
    {
        // NOTE(casey): 512mb - 2gb
        NameBase = (char *)"Giant Input";
        Divisor = 1000;
        
        Start = Mb(512);
        End = MAX_SIZE_TO_TEST;
    }
    
    meow_u64 Range = End - Start;
    for(int Index = 0;
        Index < ArrayCount(Tests->Sizes);
        ++Index)
    {
        Tests->Sizes[Index].Size = Start + (Random(&Tests->SizeSeries) % Range);
    }
    
    sprintf(Tests->Name, "[%u / %u] %s", SizeType + 1, SIZE_TYPE_COUNT, NameBase);
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
        meow_u64 ExpClocks = BestResults->ExpClocks;
        meow_u64 MaxClocks = ExpClocks + (ExpClocks/ 100);
        
        fprintf(Stream, "    ");
        PrintSize(Stream, BestResults->Size, true);
        fprintf(Stream, ": %10.0f (%6.03f bytes/cycle) - ",
                (double)BestResults->ExpClocks, (double)BestResults->ExpBPC);
        
        int TieCount = 0;
        while(ResultIndex < Tests->ResultCount)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->Size == CurSize)
            {
                if(Results->ExpClocks <= MaxClocks)
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
    
    char *CSVFileName = 0;
    if(ArgCount == 2)
    {
        CSVFileName = Args[1];
    }
    
    fprintf(stdout, "\n");
    fprintf(stdout, "meow_bench %s - basic RDTSC-based benchmark for the Meow hash\n", MEOW_HASH_VERSION_NAME);
    fprintf(stdout, "    See https://mollyrocket.com/meowhash for details\n");
    fprintf(stdout, "    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    fprintf(stdout, "             (You must turn it off in your OS if you haven't yet!)\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Versions compiled into this benchmark:\n");
    
    meow_u64 MaxClockCount = (meow_u64)10000000;
    // NOTE(mmozeiko): Test memory should be aligned to 16 because it contains member with SIMD type
    // which means that compiler can potentially issue aligned store to this SIMD member
    input_size_tests *Tests = (input_size_tests *)aligned_alloc(16, sizeof(input_size_tests));
    Tests->SizeSeries = 123456789;
    Tests->ResultCount = 0;
    
    int unsigned TypeCount = ArrayCount(NamedHashTypes);
    for(int unsigned TypeIndex = 0;
        TypeIndex < TypeCount;
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stdout, "    %d. %s\n", TypeIndex + 1, Type.FullName);
    }
    fprintf(stdout, "\n");
    
    void *Buffer = aligned_alloc(CACHE_LINE_ALIGNMENT, MAX_SIZE_TO_TEST);
    if(Buffer)
    {
        for(int unsigned SizeType = 0;
            SizeType < SIZE_TYPE_COUNT;
            ++SizeType)
        {
            int unsigned TestCount = ArrayCount(Tests->Sizes);
            
            InitializeTests(Tests, SizeType, MaxClockCount);
            fprintf(stdout, "\n----------------------------------------------------\n");
            fprintf(stdout, "\n%s\n", Tests->Name);
            fprintf(stdout, "\n----------------------------------------------------\n");
            
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
                    fprintf(stdout, "\n%s:\n", Type.FullName);
                    
                    // NOTE(casey): Clear the clock count
                    for(int unsigned SizeIndex = 0;
                        SizeIndex < ArrayCount(Tests->Sizes);
                        ++SizeIndex)
                    {
                        Tests->Sizes[SizeIndex].ClockCount = 0;
                        Tests->Sizes[SizeIndex].ClockAccum = 0;
                        Tests->Sizes[SizeIndex].ClockExp = -1ULL;
                        Tests->Sizes[SizeIndex].ClockMin = -1ULL;
                    }
                    
                    TRY
                    {
                        meow_u64 ClocksSinceLastStatus = 0;
                        meow_u64 TestRand = TestRandSeed;
                        for(int unsigned RunIndex = 0;
                            RunIndex < Tests->RunsPerHashImplementation;
                            ++RunIndex)
                        {
                            int unsigned UseIndex = Random(&TestRand);
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
                            Test->ClockCount += 1;
                            Test->ClockAccum += Clocks;
                            
                            if(Test->ClockMin > Clocks)
                            {
                                Test->ClockMin = Clocks;
                            }
                            
                            int ClocksPerAvg = 1000;
                            if(Test->ClockCount == ClocksPerAvg)
                            {
                                meow_u64 ExpClocks = Test->ClockAccum / Test->ClockCount;
                                if(Test->ClockExp > ExpClocks)
                                {
                                    Test->ClockExp = ExpClocks;
                                }
                                
                                Test->ClockAccum = 0;
                                Test->ClockCount = 0;
                            }
                            
                            ClocksSinceLastStatus += Clocks;
                            if((RunIndex == (RunsPerHashImplementation - 1)) ||
                               (ClocksSinceLastStatus > 1000000000ULL))
                            {
                                ClocksSinceLastStatus = 0;
                                fprintf(stdout, "\r    Test %0.0f / %0.0f (%0.0f%%)",
                                        (double)(RunIndex + 1), (double)RunsPerHashImplementation,
                                        100.0f * (double)(RunIndex + 1) / (double)RunsPerHashImplementation);
                                fflush(stdout);
                            }
                        }
                        fprintf(stdout, "\n");
                        
                        for(int TestIndex = 0;
                            TestIndex < TestCount;
                            ++TestIndex)
                        {
                            input_size_test *Test = Tests->Sizes + TestIndex;
                            test_results *Results = CommitResults(TypeIndex, Test, Tests);
                            if(Results)
                            {
                                fprintf(stdout, "    ");
                                PrintSize(stdout, Test->Size, true);
                                
                                fprintf(stdout, ": %0.03f bytes/cycle (%0.0f min, %0.0f exp)\n",
                                        (double)Results->ExpBPC, (double)Results->MinClocks, (double)Results->ExpClocks);
                            }
                            else
                            {
                                fprintf(stderr, "ERROR: Out of result space!\n");
                            }
                        }
                    }
                    CATCH
                    {
                        fprintf(stderr, "    (%s not supported on this CPU)\n",
                                Type.FullName);
                    }
                }
                
                //
                // NOTE(casey): Print the incremental leaderboard
                //
                PrintLeaderboard(Tests, stdout);
                fflush(stdout);
                
                //
                // NOTE(casey): Print a CSV-style section for peeps who want to graph
                //
                
                if(CSVFileName)
                {
                    FILE *CSV = fopen(CSVFileName, "w");
                    if(CSV)
                    {
                        meow_u64 LastSize = 0;
                        fprintf(CSV, "Input");
                        for(int ResultIndex = 0;
                            ResultIndex < Tests->ResultCount;
                            ++ResultIndex)
                        {
                            test_results *Results = Tests->Results + ResultIndex;
                            if(Results->Size != LastSize)
                            {
                                fprintf(CSV, ",");
                                LastSize = Results->Size;
                                PrintSize(CSV, LastSize, false);
                            }
                        }
                        fprintf(CSV, "\n");
                        
                        LastSize = 0;
                        for(int TypeIndex = 0;
                            TypeIndex < TypeCount;
                            ++TypeIndex)
                        {
                            named_hash_type Type = NamedHashTypes[TypeIndex];
                            fprintf(CSV, "%s", Type.FullName);
                            for(int ResultIndex = 0;
                                ResultIndex < Tests->ResultCount;
                                ++ResultIndex)
                            {
                                test_results *Results = Tests->Results + ResultIndex;
                                if((Results->HashType == TypeIndex) &&
                                   (Results->Size != LastSize))
                                {
                                    LastSize = Results->Size;
                                    fprintf(CSV, ",%f", Results->ExpBPC);
                                }
                            }
                            fprintf(CSV, "\n");
                        }
                        fprintf(CSV, "\n");
                        fclose(CSV);
                    }
                    else
                    {
                        fprintf(stderr, "    (unable to open %s for writing)\n",
                                CSVFileName);
                    }
                }
            }
            
            fprintf(stdout, "\n");
            fflush(stdout);
            fflush(stderr);
        }
        
        free(Buffer);
    }
    else
    {
        fprintf(stderr, "ERROR: Unable to allocate buffer for hashing\n");
    }
    
#if __aarch64__
    disable_pmu(0x008);
#endif
    
    return(0);
}
