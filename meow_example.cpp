/* ========================================================================

   meow_example.cpp - basic usage example of the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

// NOTE(casey): Meow relies on definitions for __m128/256/512, so you must
// have those defined either in your own include files or via a standard .h:
#include <intrin.h>

// NOTE(casey): We ask for all three versions here - if you only want the
// 128-bit version, you can omit the two #define's.
#define MEOW_HASH_256
#define MEOW_HASH_512
#include "meow_hash.h"

//
// NOTE(casey): Instruction-set testing
//

static meow_hash_implementation *MeowHash = MeowHash1;

int MeowHashSpecializeForCPU(void)
{
    int Result = 0;
    
#if defined(MEOW_HASH_512)
    __try
    {
        char Garbage[64];
        MeowHash4(0, sizeof(Garbage), Garbage);
        MeowHash = MeowHash4;
        Result = 512;
    }
    __except(1)
#endif
    {
#if defined(MEOW_HASH_256)
        __try
        {
            char Garbage[64];
            MeowHash2(0, sizeof(Garbage), Garbage);
            MeowHash = MeowHash2;
            Result = 256;
        }
        __except(1)
#endif
        {
            MeowHash = MeowHash1;
            Result = 128;
        }
    }
    
    return(Result);
}

//
// NOTE(casey): Usage
//

#include <stdio.h>

int main(int ArgCount, char **Args)
{
    printf("meow_example.cpp - basic usage example of the Meow hash\n");
    printf("(C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)\n");
    printf("See https://mollyrocket.com/meowhash for details.\n");
    printf("\n");
    
    // NOTE(casey): Detect which MeowHash to call - do this only once, at startup.
    int BitWidth = MeowHashSpecializeForCPU();
    printf("Using %u-bit Meow implemetation\n", BitWidth);
    
    // NOTE(casey): Make something random to hash
    int Size = 16000;
    char *Buffer = (char *)malloc(Size);
    for(int Index = 0;
        Index < Size;
        ++Index)
    {
        Buffer[Index] = (char)Index;
    }
    
    // NOTE(casey): Hash away!
    meow_lane Hash = MeowHash(0, Size, Buffer);
    
    // NOTE(casey): Extract example smaller hash sizes you might want:
    __m128i Hash128 = Hash.L0;
    log long unsigned Hash64 = Hash.Sub[0];
    int unsigned Hash32 = Hash.Sub32[0];
    
    // NOTE(casey): Print the entire 512-bit hash value using the 32-bit accessor
    // (since 64-bit printf is spec'd horribly)
    for(int Offset = 0;
        Offset < 16;
        Offset += 8)
    {
        printf("\n    %08X-%08X-%08X-%08X %08X-%08X-%08X-%08X",
               Hash.Sub32[15 - Offset],
               Hash.Sub32[14 - Offset],
               Hash.Sub32[13 - Offset],
               Hash.Sub32[12 - Offset],
               Hash.Sub32[11 - Offset],
               Hash.Sub32[10 - Offset],
               Hash.Sub32[9 - Offset],
               Hash.Sub32[8 - Offset]);
    }
    printf("\n");
}
