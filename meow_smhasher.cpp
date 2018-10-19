/* ========================================================================

   meow_smhasher.cpp - smhasher-compatible calls for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for license and details.
   
   ======================================================================== */

#include <intrin.h>

#define MEOW_HASH_256
#define MEOW_HASH_512
#include "meow_hash.h"

//
// NOTE(casey): 128-bit wide implementation (Meow1)
//

void
Meow1_32(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    *(u32 *)out = (u32)Result.Sub[0];
}

void
Meow1_64(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
}

void
Meow1_128(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
}

void
Meow1_256(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
    ((u64 *)out)[2] = Result.Sub[2];
    ((u64 *)out)[3] = Result.Sub[3];
}

//
// NOTE(casey): 256-bit wide implementation (Meow2)
//

void
Meow2_32(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    *(u32 *)out = (u32)Result.Sub[0];
}

void
Meow2_64(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
}

void
Meow2_128(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
}

void
Meow2_256(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
    ((u64 *)out)[2] = Result.Sub[2];
    ((u64 *)out)[3] = Result.Sub[3];
}

//
// NOTE(casey): 512-bit wide implementation (Meow4)
//

void
Meow4_32(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    *(u32 *)out = (u32)Result.Sub[0];
}

void
Meow4_64(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
}

void
Meow4_128(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
}

void
Meow4_256(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
    ((u64 *)out)[2] = Result.Sub[2];
    ((u64 *)out)[3] = Result.Sub[3];
}
