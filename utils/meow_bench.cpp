/* ========================================================================

   meow_bench.cpp - basic RDTSC-based benchmark for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meow_test.h"

//
// NOTE(casey): Minimalist code for testing threading.
// DO NOT use this kind of code for running Meow hash in your actual
// application!  It should be integrated properly into whatever thread pool /
// job system you are using!
//
// {
//

#if _WIN32
#include <windows.h>

static meow_macroblock_op * volatile WorkOp;
static meow_macroblock_result WorkResults[1024];
static meow_source_blocks Work;

static volatile long WorkNumber;
static volatile long CompletionNumber;
static volatile long CompletionNumberTarget;

static void
DoThreadWork(void)
{
    if(WorkNumber < CompletionNumberTarget)
    {
        int WorkUnit = _InterlockedExchangeAdd(&WorkNumber, 1);
        if(WorkUnit < CompletionNumberTarget)
        {
            meow_macroblock Macroblock = MeowGetMacroblock(&Work, WorkUnit);
            WorkResults[WorkUnit] = WorkOp(Macroblock.BlockCount, Macroblock.Source);
            _InterlockedExchangeAdd(&CompletionNumber, 1);
        }
    }
}

static DWORD WINAPI
ThreadEntry(LPVOID Parameter)
{
    for(;;)
    {
        DoThreadWork();
    }
}
#endif

//
// }
//

struct best_result
{
    meow_u64 Size;
    meow_u64 Clocks;
};

int
main(int ArgCount, char **Args)
{
    //
    // NOTE(casey): Print the banner and status
    //
    
    fprintf(stderr, "\n");
    fprintf(stderr, "meow_bench %s - basic RDTSC-based benchmark for the Meow hash\n", MEOW_HASH_VERSION_NAME);
    fprintf(stderr, "    See https://mollyrocket.com/meowhash for details\n");
    fprintf(stderr, "    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    fprintf(stderr, "             (You must turn it off in your OS if you haven't yet!)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Versions compiled into this benchmark:\n");
    for(int TypeIndex = 0;
        TypeIndex < ArrayCount(NamedHashTypes);
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stderr, "    %d. %s\n", TypeIndex + 1, Type.FullName);
    }
    
    fprintf(stderr, "\n");
    
    //
    // NOTE(casey): Check if we're testing everybody, or just Meow
    //
    int AllowOthers = !(((ArgCount == 2) && (strcmp(Args[1], "meow") == 0)));
    
    //
    // NOTE(casey): Test single-thread performance for each implementation on increasingly large input sizes
    //
    
    meow_u64 MaxClocksWithoutDrop = 4000000000ULL;
    best_result Bests[40] = {};
    double BytesPerCycle[ArrayCount(NamedHashTypes)][ArrayCount(Bests)] = {};
    
    {
        int BestIndex = 0;
        Bests[BestIndex++].Size = 1;
        Bests[BestIndex++].Size = 7;
        Bests[BestIndex++].Size = 8;
        Bests[BestIndex++].Size = 15;
        Bests[BestIndex++].Size = 16;
        Bests[BestIndex++].Size = 31;
        Bests[BestIndex++].Size = 32;
        Bests[BestIndex++].Size = 63;
        Bests[BestIndex++].Size = 64;
        Bests[BestIndex++].Size = 127;
        Bests[BestIndex++].Size = 128;
        Bests[BestIndex++].Size = 255;
        Bests[BestIndex++].Size = 256;
        Bests[BestIndex++].Size = 511;
        Bests[BestIndex++].Size = 512;
        Bests[BestIndex++].Size = 1023;
        Bests[BestIndex++].Size = 1024;
        meow_u64 Size = Bests[BestIndex - 1].Size;
        while(BestIndex < ArrayCount(Bests))
        {
            Size *= 2;
            Bests[BestIndex++].Size = Size;
        }
    }
    
    {
        fprintf(stderr, "Single-threaded performance:\n");
        for(int Batch = 0;
            Batch < ArrayCount(Bests);
            ++Batch)
        {
            best_result *ThisBest = Bests + Batch;
            meow_u64 Size = ThisBest->Size;
            ThisBest->Clocks = (meow_u64)-1ULL;
            
            void *Buffer = aligned_alloc(MEOW_HASH_ALIGNMENT, Size);
            if(Buffer)
            {
                fprintf(stderr, "  Fewest cycles to hash ");
                PrintSize(stderr, Size, false);
                fprintf(stderr, ":\n");
                
                for(int TypeIndex = 0;
                    TypeIndex < ArrayCount(NamedHashTypes);
                    ++TypeIndex)
                {
                    named_hash_type Type = NamedHashTypes[TypeIndex];
                    if(AllowOthers || Type.Op)
                    {
                        TRY
                        {
                            meow_u64 ClocksSinceLastDrop = 0;
                            meow_u64 BestClocks = (meow_u64)-1ULL;
                            int TryIndex = 0;
                            while((TryIndex < 10) || (ClocksSinceLastDrop < MaxClocksWithoutDrop))
                            {
                                meow_u64 StartClock = __rdtsc();
                                Type.Imp(0, Size, Buffer);
                                meow_u64 EndClock = __rdtsc();
                                
                                meow_u64 Clocks = EndClock - StartClock;
                                ClocksSinceLastDrop += Clocks;
                                
                                if(BestClocks > Clocks)
                                {
                                    ClocksSinceLastDrop = 0;
                                    BestClocks = Clocks;
                                }
                                
                                ++TryIndex;
                            }
                            
                            double BPC = (double)Size / (double)BestClocks;
                            fprintf(stderr, "    %32s %10.0f (%3.03f bytes/cycle)\n",
                                    Type.FullName, (double)BestClocks, BPC);
                            fflush(stderr);
                            
                            BytesPerCycle[TypeIndex][Batch] = BPC;
                            
                            if(ThisBest->Clocks > BestClocks)
                            {
                                ThisBest->Clocks = BestClocks;
                            }
                        }
                        CATCH
                        {
                            if(Batch == 0)
                            {
                                fprintf(stderr, "(%s not supported on this CPU)\n",
                                        Type.FullName);
                                Type.Op = 0;
                            }
                        }
                    }
                }
                
                free(Buffer);
            }
        }
    }
    
    fprintf(stderr, "\n");
    
    //
    // NOTE(casey): Print the single-thread leaderboard (whichever hash was fastest)
    //
    
    fprintf(stderr, "Leaderboard:\n");
    for(int BestIndex = 0;
        BestIndex < ArrayCount(Bests);
        ++BestIndex)
    {
        best_result *Best = Bests + BestIndex;
        fprintf(stderr, "  ");
        PrintSize(stderr, Best->Size, true);
        double BPC = (double)Best->Size / (double)Best->Clocks;
        fprintf(stderr, ": %10.0f (%3.03f bytes/cycle) - ",
                (double)Best->Clocks, BPC);
        
        int TieCount = 0;
        for(int TypeIndex = 0;
            TypeIndex < ArrayCount(NamedHashTypes);
            ++TypeIndex)
        {
            if(BPC == BytesPerCycle[TypeIndex][BestIndex])
            {
                named_hash_type Type = NamedHashTypes[TypeIndex];
                
                if(TieCount)
                {
                    fprintf(stderr, ", ");
                }
                
                fprintf(stderr, "%s", Type.FullName);
                
                ++TieCount;
            }
        }
        if(TieCount > 1)
        {
            fprintf(stderr, " (%d-way tie)", TieCount);
        }
        fprintf(stderr, "\n");
    }
    
    fprintf(stderr, "\n");
    
    //
    // NOTE(casey): Print a CSV-style section for peeps who want to graph
    //

    fprintf(stdout, "Input");
    for(int TypeIndex = 0;
        TypeIndex < ArrayCount(NamedHashTypes);
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stdout, ",%s", Type.FullName);
    }
    fprintf(stdout, "\n");
    
    for(int BestIndex = 0;
        BestIndex < ArrayCount(Bests);
        ++BestIndex)
    {
        best_result *Best = Bests + BestIndex;
        PrintSize(stdout, Best->Size, false);
        
        for(int TypeIndex = 0;
            TypeIndex < ArrayCount(NamedHashTypes);
            ++TypeIndex)
        {
            fprintf(stdout, ",%f", BytesPerCycle[TypeIndex][BestIndex]);
        }
        fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    fflush(stdout);
    
#if _WIN32
    //
    // NOTE(casey): Test multiple-thread performance for each Meow implementation
    //
    
    {
        SYSTEM_INFO SystemInfo = {};
        GetSystemInfo(&SystemInfo);
        int MaxThreadCount = SystemInfo.dwNumberOfProcessors + 2;
        
        int Size = 1024*1024*1024;
        fprintf(stderr, "Multi-threaded performance (Meow only):\n");
        for(int ThreadCount = 1;
            ThreadCount < MaxThreadCount;
            ++ThreadCount)
        {
            void *Buffer = aligned_alloc(MEOW_HASH_ALIGNMENT, Size);
            meow_hash ReferenceHash = MeowHash1(0, Size, Buffer);
            Work = MeowSourceBlocksFor(Size, Buffer);
            
            for(int TypeIndex = 0;
                TypeIndex < ArrayCount(NamedHashTypes);
                ++TypeIndex)
            {
                named_hash_type Type = NamedHashTypes[TypeIndex];
                if(Type.Op)
                {
                    WorkOp = Type.Op;
                    meow_u64 BestClocks = (meow_u64)-1ULL;
                    for(int Test = 0;
                        Test < 100;
                        ++Test)
                    {
                        InterlockedAnd(&CompletionNumberTarget, 0);
                        _ReadWriteBarrier(); _mm_mfence();
                        CompletionNumber = 0;
                        WorkNumber = 0;
                        _ReadWriteBarrier(); _mm_mfence();
                        
                        meow_u64 StartClock = __rdtsc();
                        CompletionNumberTarget = Work.MacroblockCount;
                        while(CompletionNumber < CompletionNumberTarget)
                        {
                            DoThreadWork();
                        }
                        
                        _ReadWriteBarrier();
                        
                        // TODO(casey): Technically I would like to force a re-read of WorkResults here.
                        // I don't really know how to convince the compiler to do that.  Currently it does,
                        // but it'd be nice to guard against a day when it learns to optimize it out.  I
                        // want something like "this is a barrier where you have to treat it as volatile
                        // between before and after the barrier", which I don't think exists?  Like a
                        // "consider_this_pointers_memory_changed_as_of_right_now()" intrinsic.
                        
                        meow_macroblock_result Group = MeowHashMergeArray(Work.MacroblockCount, WorkResults);
                        meow_hash Hash = MeowHashFinish(&Group, 0, Work.TotalLengthInBytes, Work.Overhang, Work.OverhangStart);
                        
                        _ReadWriteBarrier();
                        
                        meow_u64 EndClock = __rdtsc();
                        
                        if(!MeowHashesAreEqual(Hash, ReferenceHash))
                        {
                            fprintf(stderr, "ERROR: Multithreaded hash failed to produce the same hash as the single-threaded hash!\n");
                            ExitProcess(0);
                        }
                        
                        meow_u64 Clocks = EndClock - StartClock;
                        if(BestClocks > Clocks)
                        {
                            BestClocks = Clocks;
                            fprintf(stderr, "\r%s fastest hash of ", Type.FullName);
                            PrintSize(stderr, Size, false);
                            fprintf(stderr, " on %u thread%s: %0.0f (%f bytes/cycle)               ",
                                    ThreadCount, (ThreadCount == 1) ? " " : "s", (double)BestClocks, (double)Size / (double)BestClocks);
                            fflush(stderr);
                            Test = 0;
                        }
                    }
                    fprintf(stderr, "\n");
                }
            }
            
            free(Buffer);
        
            DWORD ThreadID;
            CloseHandle(CreateThread(0, 0, ThreadEntry, 0, 0, &ThreadID));
        }
    }
    
    ExitProcess(0);
#endif
    
    return(0);
}
