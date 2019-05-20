/* ========================================================================

   meow_test.cpp - basic sanity checking for any build of Meow hash
   (C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#undef MEOW_INCLUDE_C
#undef MEOW_INCLUDE_TRUNCATIONS
#undef MEOW_INCLUDE_OTHER_HASHES

#define MEOW_INCLUDE_C 1
#define MEOW_INCLUDE_TRUNCATIONS 0
#define MEOW_INCLUDE_OTHER_HASHES 0

#define MEOW_DUMP 1
#include "meow_test.h"

#ifdef _MSC_VER
#include <windows.h>

static void * OS_PageAlloc( void ) // allocate a page of memory with a guard page after it
{
    void * p;
    unsigned long ignored;
    
    p = VirtualAlloc( 0, MEOW_PAGESIZE*3,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    // protect first page
    VirtualProtect( p, MEOW_PAGESIZE,PAGE_NOACCESS, &ignored );
    // protect last page
    VirtualProtect( ( (char*)p ) + MEOW_PAGESIZE*2, MEOW_PAGESIZE,PAGE_NOACCESS, &ignored );
    
    // return middle page
    return ( (char*)p ) + MEOW_PAGESIZE;
}

static void OS_PageFree( void * ptr )
{
  VirtualFree( ( (char*)ptr ) - MEOW_PAGESIZE,0,MEM_RELEASE);
}
#else
// TODO(casey): Page-guarded alloc/free for Linux/Mac w/ mmap/mprotect/etc.
static void * OS_PageAlloc( void ) // allocate a page of memory with a guard page after it
{
    void * p = malloc(MEOW_PAGESIZE);
    return ( p );
}

static void OS_PageFree( void * ptr )
{
    free(ptr);
}
#endif

//
// NOTE(casey): Minimalist code for Meow testing.
//
// This is NOT a replacement for the real hash testing (done via smhasher, etc.)
// It is just a brief sanity check to ensure that your Meow compilation is
// working correctly.
//

static void
DiffStates(char const *NameA, meow_u32 CanonicalDumpCount, meow_dump *CanonicalDump,
           char const *NameB, meow_u32 TestDumpCount, meow_dump *TestDump)
{
    if(CanonicalDumpCount == TestDumpCount)
    {
        bool Continue = true;
        for(int D = 0;
            (D < TestDumpCount);
            ++D)
        {
            meow_dump *Can = CanonicalDump + D;
            meow_dump *Test = TestDump + D;
            printf("\n%s/%s:\n", Can->Title, Test->Title);
            for(int E = 0;
                E < ArrayCount(Test->xmm);
                ++E)
            {
                meow_u32 *VC = (meow_u32 *)&Can->xmm[E];
                meow_u32 *VT = (meow_u32 *)&Test->xmm[E];
                bool Same = ((VC[0] == VT[0]) &&
                             (VC[1] == VT[1]) &&
                             (VC[2] == VT[2]) &&
                             (VC[3] == VT[3]));
                printf("  xmm%02u %s\n", E, Same ? "same" : "DIFF");
                printf("         0x%08x 0x%08x 0x%08x 0x%08x (%s)\n", VC[3], VC[2], VC[1], VC[0], NameA);
                printf("         0x%08x 0x%08x 0x%08x 0x%08x (%s)\n", VT[3], VT[2], VT[1], VT[0], NameB);
            }
            
            printf("  ptr   %s:\n", (Can->Ptr == Test->Ptr) ? "same" : "DIFF");
            printf("         0x%8p (%s)\n", Can->Ptr, NameA);
            printf("         0x%8p (%s)\n", Test->Ptr, NameB);
        }
    }
    else
    {
        printf("\nDIFF: State dumps differ in size (canonical: %u, test: %u)\n", (meow_u32)CanonicalDumpCount, (meow_u32)TestDumpCount);
    }
}

int
main(int ArgCount, char **Args)
{
    int Result = 0;
    
    // NOTE(casey): Print the banner
    printf("meow_test %s - basic sanity test for a Meow hash build\n", MEOW_HASH_VERSION_NAME);
    printf("    See https://mollyrocket.com/meowhash for details\n");
    printf("\n");

    printf("Unaligned sources: ");
    {
        meow_u8 *Test = (meow_u8 *)aligned_alloc(CACHE_LINE_ALIGNMENT, 257);
        TRY
        {
            MeowHash(MeowDefaultSeed, 256, Test + 1);
            printf("supported");
        }
        CATCH
        {
            printf("UNSUPPORTED");
        }
        free(Test);
    }
    printf("\n");
    
    meow_u32 CanonicalDumpCount = 0;
    meow_dump CanonicalDump[32] = {};
    meow_u32 TestDumpCount = 0;
    meow_dump TestDump[32] = {};

    meow_u8 Seeds[4][128] = {};
    memcpy(Seeds[2], MeowDefaultSeed, 128);
    meow_u64 BadSeed0 = 0;
    meow_u64 BadSeed1 = 0x01234567;
    MeowExpandSeed(sizeof(BadSeed0), &BadSeed0, Seeds[2]);
    MeowExpandSeed(sizeof(BadSeed1), &BadSeed1, Seeds[3]);

    meow_u8 StateBuffer[1024];
    for(int TypeIndex = 0;
        TypeIndex < ArrayCount(NamedHashTypes);
        ++TypeIndex)
    {
        named_hash_type *Type = NamedHashTypes + TypeIndex;

        int TotalPossible = 0;
        int ImpError = 0;
        int StreamError = 0;
        int Unsupported = 0;
        int MaxBufferSize = 2048;

        for(int SeedIndex = 0;
            SeedIndex < ArrayCount(Seeds);
            ++SeedIndex)
        {
            meow_u8 *Seed128 = Seeds[SeedIndex];
            
            for(int BufferSize = 1;
                BufferSize <= MaxBufferSize;
                ++BufferSize)
            {
                int AllocationSize = BufferSize + 2*CACHE_LINE_ALIGNMENT;
                meow_u8 *Allocation = (meow_u8 *)aligned_alloc(CACHE_LINE_ALIGNMENT, AllocationSize);
                memset(Allocation, 0, AllocationSize);
                
                meow_u8 *Buffer = Allocation + CACHE_LINE_ALIGNMENT;
                for(int Guard = 0;
                    Guard < 1;
                    ++Guard)
                {
                    for(int Flip = 0;
                        Flip < (8*BufferSize);
                        ++Flip)
                    {
                        meow_u8 *FlipByte = Buffer + (Flip / 8);
                        meow_u8 FlipBit = (1 << (Flip % 8));
                        *FlipByte |= FlipBit;
                        
                        meow_u128 Canonical = {};
                        if(Type->Reference)
                        {
                            MeowDumpTo = CanonicalDump;
                            Canonical = Type->Reference(Seed128, BufferSize, Buffer);
                            CanonicalDumpCount = MeowDumpTo - CanonicalDump;
                            MeowDumpTo = 0;
                        }
                        
                        if(Guard)
                        {
                            memset(Allocation, 0xFF, CACHE_LINE_ALIGNMENT);
                            memset(Allocation + CACHE_LINE_ALIGNMENT + BufferSize, 0xFF, CACHE_LINE_ALIGNMENT);
                        }
                        
                        ++TotalPossible;
                        TRY
                        {
                            MeowDumpTo = TestDump;
                            meow_u128 ImpHash = Type->Imp(Seed128, BufferSize, Buffer);
                            TestDumpCount = MeowDumpTo - TestDump;
                            MeowDumpTo = 0;
                            
                            if(Type->Reference && !MeowHashesAreEqual(Canonical, ImpHash))
                            {
                                DiffStates("Canonical", CanonicalDumpCount, CanonicalDump,
                                           "Test", TestDumpCount, TestDump);
                                ++ImpError;
                            }
                            
                            if(Type->Absorb)
                            {
                                for(int SplitTest = 0;
                                    SplitTest < 10;
                                    ++SplitTest)
                                {
                                    MeowDumpTo = TestDump;
                                    Type->Begin(StateBuffer, Seed128);
                                    
                                    meow_u8 *At = Buffer;
                                    int unsigned Count = BufferSize;
                                    while(Count)
                                    {
                                        int unsigned Amount = rand() % (BufferSize + 1);
                                        if(Amount > Count)
                                        {
                                            Amount = Count;
                                        }
                                        
                                        Type->Absorb(StateBuffer, Amount, At);
                                        At += Amount;
                                        Count -= Amount;
                                    }
                                    
                                    meow_u128 AbsorbHash = Type->End(StateBuffer, 0);
                                    TestDumpCount = MeowDumpTo - TestDump;
                                    MeowDumpTo = 0;
                                    
                                    if(Type->Reference && !MeowHashesAreEqual(Canonical, AbsorbHash))
                                    {
                                        DiffStates("Canonical", CanonicalDumpCount, CanonicalDump,
                                                   "Test", TestDumpCount, TestDump);
                                        ++StreamError;
                                        break;
                                    }
                                }
                            }
                        }
                        CATCH
                        {
                            ++Unsupported;
                            break;
                        }
                        
                        if(Guard)
                        {
                            memset(Allocation, 0, CACHE_LINE_ALIGNMENT);
                            memset(Allocation + CACHE_LINE_ALIGNMENT + BufferSize, 0, CACHE_LINE_ALIGNMENT);
                        }
                        
                        *FlipByte &= ~FlipBit;
                        
                        if(Type->Reference)
                        {
                            MeowDumpTo = TestDump;
                            meow_u128 OppositeHash = Type->Reference(Seed128, BufferSize, Buffer);
                            TestDumpCount = MeowDumpTo - TestDump;
                            MeowDumpTo = 0;
                            
                            for(int LaneCheck = 0;
                                LaneCheck < 4;
                                ++LaneCheck)
                            {
                                if(((meow_u32 *)&OppositeHash)[LaneCheck] == ((meow_u32 *)&Canonical)[LaneCheck])
                                {
                                    printf("\nCOLLISION: buffer size %d with bit %d flipped collides on lane %d\n", BufferSize, Flip, LaneCheck);
                                    DiffStates("Bit=1", CanonicalDumpCount, CanonicalDump,
                                               "Bit=0", TestDumpCount, TestDump);
                                    break;
                                }
                            }
                        }
                    }
                }
                fprintf(stderr, "\r%s/seed%u: (%0.0f%%)   ", Type->FullName, SeedIndex,
                        (double)BufferSize * 100.0f / (double)MaxBufferSize);
                
                
                free(Allocation);
            }
            
            fprintf(stderr, "\r%s/seed%u: ", Type->FullName, SeedIndex);
            
            if(Unsupported)
            {
                printf("UNSUPPORTED");
            }
            else
            {
                if(ImpError || StreamError)
                {
                    printf("FAILED");
                    if(ImpError)
                    {
                        printf(" [direct:%u/%u]", ImpError, TotalPossible);
                    }
                    
                    if(StreamError)
                    {
                        printf(" [stream:%u/%u]", StreamError, TotalPossible);
                    }
                    
                    Result = -1;
                }
                else
                {
                    printf("PASSED");
                }
            }
            printf("\n");
        }
    }
    
    printf("\n\nTesting reading right up to page size.\n");
    for(int TypeIndex = 0;
        TypeIndex < ArrayCount(NamedHashTypes);
        ++TypeIndex)
    {
        named_hash_type *Type = NamedHashTypes + TypeIndex;
        meow_u8 *Allocation = (meow_u8 *)OS_PageAlloc();
        int length;
        
        memset( Allocation, 0x88, MEOW_PAGESIZE );
        
        for ( length = 0 ; length <= 16 ; length++ )
        {
            int i;
            
            memset( Allocation, 0x27, length );
            meow_u128 Canonical = {};
            if(Type->Reference)
            {
                MeowDumpTo = CanonicalDump;
                Canonical = Type->Reference(MeowDefaultSeed, length, Allocation);
                CanonicalDumpCount = MeowDumpTo - CanonicalDump;
                MeowDumpTo = 0;
            }
            
            for( i = (MEOW_PAGESIZE-32); i <= (MEOW_PAGESIZE-length) ; i++ )
            {
                int crash = 0;

                memset( Allocation + (MEOW_PAGESIZE-32), 0xff, 32 );
                memset( Allocation + i, 0x27, length );
                
                meow_u128 ImpHash;
                TRY
                {
                    MeowDumpTo = TestDump;
                    ImpHash = Type->Imp(MeowDefaultSeed, length, Allocation + i);
                    TestDumpCount = MeowDumpTo - TestDump;
                    MeowDumpTo = 0;
                }
                CATCH
                {
                    crash = 1;
                    printf( "%s: Crash at offset: %d with byte length: %d\n", Type->FullName, i-(MEOW_PAGESIZE-32), length );
                }
                
                if(Type->Reference && !MeowHashesAreEqual(Canonical, ImpHash))
                {
                    printf( "%s:  Mismatch to canonical at offset: %d with byte length: %d\n", Type->FullName, i-(MEOW_PAGESIZE-32), length );
                    DiffStates("Canonical", CanonicalDumpCount, CanonicalDump,
                               "Test", TestDumpCount, TestDump);
                }
            }
        }
        OS_PageFree( Allocation );
    }
    
    printf("  Done.\n");

    return(Result);
}
