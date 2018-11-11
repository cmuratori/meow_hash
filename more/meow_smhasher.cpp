/* ========================================================================

   meow_smhasher.cpp - smhasher-compatible calls for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for license and details.
   
   ======================================================================== */

#include "meow_intrinsics.h"
#include "meow_hash.h"

//
// NOTE(casey): 128-bit wide implementation (Meow1)
//

void
Meow1_32(const void * key, int len, meow_u32 seed, void * out)
{
    meow_u128 Result = MeowHash_Accelerated(seed, len, (void *)key);
    *(meow_u32 *)out = MeowU32From(Result, 0);
}

void
Meow1_64(const void * key, int len, meow_u32 seed, void * out)
{
    meow_u128 Result = MeowHash_Accelerated(seed, len, (void *)key);
    ((meow_u64 *)out)[0] = MeowU64From(Result, 0);
}

void
Meow1_128(const void * key, int len, meow_u32 seed, void * out)
{
    meow_u128 Result = MeowHash_Accelerated(seed, len, (void *)key);
    ((meow_u64 *)out)[0] = MeowU64From(Result, 0);
    ((meow_u64 *)out)[1] = MeowU64From(Result, 1);
}
