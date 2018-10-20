/* ========================================================================

   meow_smhasher.cpp - smhasher-compatible calls for the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for license and details.
   
   ======================================================================== */

#include <intrin.h>

#include "meow_hash.h"

#include <string.h>

//
// NOTE(casey): 128-bit wide implementation (Meow1)
//

void
Meow1_32(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    memcpy(out, &Result, 4);
}

void
Meow1_64(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    memcpy(out, &Result, 8);
}

void
Meow1_128(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    memcpy(out, &Result, 16);
}

void
Meow1_256(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash1(seed, len, (void *)key);
    memcpy(out, &Result, 32);
}

//
// NOTE(casey): 256-bit wide implementation (Meow2)
//

#if defined(MEOW_HASH_256)
void
Meow2_32(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    memcpy(out, &Result, 4);
}

void
Meow2_64(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    memcpy(out, &Result, 8);
}

void
Meow2_128(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    memcpy(out, &Result, 16);
}

void
Meow2_256(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash2(seed, len, (void *)key);
    memcpy(out, &Result, 32);
}
#endif

//
// NOTE(casey): 512-bit wide implementation (Meow4)
//

#if defined(MEOW_HASH_512)
void
Meow4_32(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    memcpy(out, &Result, 4);
}

void
Meow4_64(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    memcpy(out, &Result, 8);
}

void
Meow4_128(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    memcpy(out, &Result, 16);
}

void
Meow4_256(const void * key, int len, meow_u32 seed, void * out)
{
    meow_lane Result = MeowHash4(seed, len, (void *)key);
    memcpy(out, &Result, 32);
}
#endif
