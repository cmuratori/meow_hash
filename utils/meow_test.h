/* ========================================================================

   meow_test.h - shared functions for Meow testing utilities
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include "meow_intrinsics.h" // NOTE(casey): Platform prerequisites for the Meow hash code (replace with your own, if you want)

// NOTE(casey): When we're doing benchmarking, we look for optimal speeds by aligning
// to the Xeon "double cache line alignment", just to make sure all hashes have the
// best chance at performing well.
#define CACHE_LINE_ALIGNMENT 128

#if _MSC_VER
#define TRY __try
#define CATCH __except(1)
#else
#define TRY try
#define CATCH catch(...)
#endif

#if __APPLE__
// NOTE: Apple Xcode/clang seems to not include aligned_alloc in the standard
// library, so emulate via posix_memalign.
static void* aligned_alloc(size_t alignment, size_t size)
{
    void* pointer = 0;
    posix_memalign(&pointer, alignment, size);
    return pointer;
}
#elif _WIN32
// NOTE(mmozeiko): MSVC/gcc/clang on Windows should use _aligned_...
// functions from functions stdlib.h
#include <stdlib.h>
#define malloc(a) _aligned_malloc(a,16)
#define aligned_alloc(a,b) _aligned_malloc(b,a)
#define free _aligned_free
#endif

#include "meow_hash.h"

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

static meow_u128
MeowHashTruncate64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = MeowHash1(Seed, Len, Source);
    ((meow_u64 *)&Result)[1] = 0;
    return(Result);
}

static meow_u128
MeowHashTruncate32(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = MeowHashTruncate64(Seed, Len, Source);
    ((meow_u32 *)&Result)[1] = 0;
    return(Result);
}

//
// NOTE(casey): To avoid having to comply with the notice provisions of
// lots of different licenses, other hashes for comparison are NOT
// included in the Meow repository.  However, if you download them
// yourself, massage them into working together, and put them in an "other"
// directory, you can enable these bindings to automatically include
// them in the Meow utilities where applicable.
//

#if MEOW_INCLUDE_OTHER_HASHES

#include "other/falkhash.c"
static meow_u128
FalkHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    Result = falkhash(Source, Len, Seed);
    
    return(Result);
}

#include "other/metrohash128.h"
#include "other/metrohash128.cpp"
static meow_u128
MetroHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    MetroHash128::Hash((uint8_t *)Source, Len, (uint8_t *)&Result, Seed);
    
    return(Result);
}

#include "other/t1ha0_ia32aes_avx.c"
static meow_u128
t1ha64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(meow_u64 *)&Result = t1ha0_ia32aes_avx(Source, Len, Seed);
    
    return(Result);
}

#include "other/xxhash.c"
static meow_u128
xxHash64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(meow_u64 *)&Result = XXH64(Source, Len, Seed);
    
    return(Result);
}

#include "other/clhash.c"
static void *CLHashJunk;
static meow_u128
CLHash64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    // NOTE(casey): We cheat here and allow CLHash to re-use its "junk", even though this
    // is technically illegal because the benchmark expects you to take a seed.  Everyone
    // else just handles that, whereas CLHash has a tremendous overhead cost for changing
    // the seed.
    *(meow_u64 *)&Result = clhash(CLHashJunk, (char *)Source, Len);
    
    return(Result);
}

#include "other/highwayhash.h"
#include "other/highwayhash.c"
union highway_result
{
    meow_u128 Meow;
    uint64_t Hash[2];
};
static meow_u128
HighwayHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    highway_result Result;
    
    uint64_t Key[4] = {Seed, Seed, Seed, Seed};
    HighwayHash128((uint8_t *)Source, Len, Key, Result.Hash);
    
    return(Result.Meow);
}

#include "other/city.h"
#include "other/city.cc"
static meow_u128
CityHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(uint128 *)&Result = CityHash128((char *)Source, Len);
    
    return(Result);
}

#define FARMHASH_NO_BUILTIN_EXPECT // NOTE(casey): It appears Farm Hash doesn't support __assume, only __builtin_expect, I guess?
#include "other/farmhash.cc"
static meow_u128
FarmHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_u128 Result;
    
    /* NOTE(casey): I know it LOOKS like this is calling CityHash, but it's not - Farm's 128-bit hash is defined like this:
    
       return DebugTweak(farmhashcc::CityHash128WithSeed(s, len, seed));
       
       And then inside their CityHash128WithSeed, they call Murmur on small values, etc.  So this just goes more directly
       to the source (hopefully - oh my god Farm Hash's code is giant)
    */
    
    util::uint128_t R = util::farmhashcc::CityHash128WithSeed((const char *)Source, (size_t)Len, util::Uint128(Seed, Seed));
    
    Result = Meow128_Set64x2(R.first, R.second);
    return(Result);
}

#endif

//
// NOTE(casey): List of available hash implementations
//

struct named_hash_type
{
    char *ShortName;
    char *FullName;
    
    meow_hash_implementation *Imp;
    meow_absorb_implementation *Absorb;
};

static named_hash_type NamedHashTypes[] =
{
#define MEOW_HASH_TEST_INDEX_128 0
    {(char *)"Meow128", (char *)"Meow 128-bit AES-NI 128-wide", MeowHash1, MeowHashAbsorb1},
#if MEOW_HASH_AVX512
    {(char *)"Meow128x2", (char *)"Meow 128-bit VAES 256-wide", MeowHash2, MeowHashAbsorb2},
    {(char *)"Meow128x4", (char *)"Meow 128-bit VAES 512-wide", MeowHash4, MeowHashAbsorb4},
#endif
#if MEOW_INCLUDE_TRUNCATIONS
    {(char *)"Meow64", (char *)"Meow 64-bit AES-NI 128-wide", MeowHashTruncate64},
    {(char *)"Meow32", (char *)"Meow 32-bit AES-NI 128-wide", MeowHashTruncate32},
#endif

#if MEOW_INCLUDE_OTHER_HASHES
    {(char *)"t1ha64", (char *)"t1ha 64-bit", t1ha64},
    {(char *)"Falk128", (char *)"Falk Hash 128-bit", FalkHash128},
    {(char *)"xx64", (char *)"xxHash 64-bit", xxHash64},
    {(char *)"Met128", (char *)"Metro Hash 128-bit", MetroHash128},
    {(char *)"City128", (char *)"City Hash 128-bit", CityHash128},
    {(char *)"Farm", (char *)"Farm Hash 128-bit", FarmHash128},
    {(char *)"CL", (char *)"CLHash 64-bit", CLHash64},
    
    // NOTE(casey): Highway Hash is disabled until someone provides a usable ~4 file implementation
    // that is optimized.
//    {(char *)"High128", (char *)"Highway Hash 128-bit", HighwayHash128},
#endif
};

static void
PrintSize(FILE *Stream, double Size, int Fixed)
{
    int Decimals = 0;
    
    char *Suffix = Fixed ? (char *)"b " : (char *)"b";
    if(Size >= 1024.0)
    {
        Suffix = (char *)"kb";
        Size /= 1024.0;
        if(Size >= 1024.0)
        {
            Decimals = 1;
            
            Suffix = (char *)"mb";
            Size /= 1024.0;
            if(Size >= 1024.0)
            {
                Suffix = (char *)"gb";
                Size /= 1024.0;
            }
        }
    }
  
    if(Decimals)
    {
        fprintf(Stream, Fixed ? "%6.2f%s" : "%0.2f%s", Size, Suffix);
    }
    else
    {
        fprintf(Stream, Fixed ? "%6.0f%s" : "%0.0f%s", Size, Suffix);
    }
}

static void
PrintHash(FILE *Stream, meow_u128 Hash)
{
    meow_u32 *HashU32 = (meow_u32 *)&Hash;
    fprintf(Stream, "%08X-%08X-%08X-%08X", HashU32[3], HashU32[2], HashU32[1], HashU32[0]);
}

static void
InitializeHashesThatNeedInitializers(void)
{
#if MEOW_INCLUDE_OTHER_HASHES
    CLHashJunk = get_random_key_for_clhash(1234, 5678);
#endif
}
