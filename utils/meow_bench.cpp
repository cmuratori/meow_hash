/* ========================================================================

   meow_bench.cpp - basic RDTSC-based benchmark for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   See accompanying meow.inl for license and usage.
   
   ======================================================================== */

#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>

// TODO(casey): At some point, it would be nice to port this to non-Windows /
// CLANG x64, which supports rdtsc as well, but I need to include some "define
// cracking" here to test which one we're on and get the right intrinsics and
// include the right stuff.
#include <windows.h>

#define MEOW_HASH_256
#define MEOW_HASH_512
#include "meow_hash.h"

struct named_specialization
{
    char *Name;
    meow_hash_implementation *Handler;
};

static named_specialization Specializations[] =
{
#if defined(MEOW_HASH_512)
    {"MeowHash AVX-512 VAES", MeowHash4},
#endif
#if defined(MEOW_HASH_256)
    {"MeowHash AVX-256 VAES", MeowHash2},
#endif
    {"MeowHash SSE AES-NI  ", MeowHash1},
};

void
main(int ArgCount, char **Args)
{
    printf("\n");
    printf("meow_bench %s - basic RDTSC-based benchmark for the Meow hash\n", MEOW_HASH_VERSION_NAME);
    printf("    See https://mollyrocket.com/meowhash for details\n");
    printf("    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    printf("\n");
    printf("Versions compiled into this benchmark:\n");
    for(int SpecializationIndex = 0;
        SpecializationIndex < (sizeof(Specializations)/sizeof(Specializations[0]));
        ++SpecializationIndex)
    {
        named_specialization Specialization = Specializations[SpecializationIndex];
        printf("    %d. %s\n", SpecializationIndex + 1, Specialization.Name);
    }
    
    printf("\n");
    printf("Test results:\n");
    int Size = 32*1024;
    int TestCount = 8000000;
    for(int Batch = 0;
        Batch < 16;
        ++Batch)
    {
        void *Buffer = _aligned_malloc(Size, 128);
        
        for(int SpecializationIndex = 0;
            SpecializationIndex < (sizeof(Specializations)/sizeof(Specializations[0]));
            ++SpecializationIndex)
        {
            named_specialization Specialization = Specializations[SpecializationIndex];
            __try
            {
                meow_u64 BestClocks = (meow_u64)-1;
                for(int Test = 0;
                    Test < TestCount;
                    ++Test)
                {
                    meow_u64 StartClock = __rdtsc();
                    Specialization.Handler(0, Size, Buffer);
                    meow_u64 EndClock = __rdtsc();
                    
                    meow_u64 Clocks = EndClock - StartClock;
                    if(BestClocks > Clocks)
                    {
                        printf("\r%s least cycles to hash %uk: %u (%f bytes/cycle)               ",
                               Specialization.Name,
                               (Size/1024), (int unsigned)BestClocks, (double)Size / (double)BestClocks);
                        Test = 0;
                        BestClocks = Clocks;
                    }
                }
                printf("\n");
            }
            __except(1)
            {
                if(Batch == 0)
                {
                    printf("(%s not supported on this CPU)\n",
                           Specialization.Name);
                }
            }
        }
        
        _aligned_free(Buffer);
        
        Size *= 2;
        TestCount /= 2;
    }
}
