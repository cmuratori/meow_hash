/* ========================================================================

   meow_test.h - shared functions for Meow testing utilities
   (C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

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

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))
#include "meow_hash_x64_aesni.h"
#define meow_u32 int unsigned

static meow_u128
MeowHash128(void *Seed128, meow_u64 Len, void *Source)
{
    // NOTE(casey): This call is just a thunk, but it needs to be here to ensure a "fair playing field".
    // If everyone _else_ has to go through a thunk, we should too, to avoid having unfair timings.
    meow_u128 Result = MeowHash(Seed128, Len, Source);
    return(Result);
}

static meow_u128
MeowHashTruncate64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = MeowHash(Seed128, Len, Source);
    ((meow_u64 *)&Result)[1] = 0;
    return(Result);
}

static meow_u128
MeowHashTruncate32(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = MeowHashTruncate64(Seed128, Len, Source);
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

#include "other/wyhash.h"
static meow_u128
Wyhash64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    *(meow_u64 *)&Result = wyhash(Source, Len, *(meow_u64 *)Seed128);
    return(Result);
}

#include "other/SpookyV2.cpp"
static meow_u128
Spooky128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    // NOTE(casey): I felt it was necessary to actually copy the seed here, because otherwise Spooky is getting a "free pass" on
    // its calling convention of having to pass the seed in place of the destination, which it shouldn't get (all other hashes
    // have to do this copy internally themselves).
    Result = *(meow_u128 *)Seed128;
    meow_u64 *U = (meow_u64 *)&Result;
    SpookyHash::Hash128(Source, Len, U + 0, U + 1);
    
    return(Result);
}

#include "other/mum.h"
static meow_u128
Mum64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(meow_u64 *)&Result = mum_hash(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/falkhash.c"
static meow_u128
FalkHash128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    Result = falkhash(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/metrohash128.h"
#include "other/metrohash128.cpp"
static meow_u128
MetroHash128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    MetroHash128::Hash((uint8_t *)Source, Len, (uint8_t *)&Result, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/t1ha0_ia32aes_avx.c"
static meow_u128
t1ha64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(meow_u64 *)&Result = t1ha0_ia32aes_avx(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/xxhash.c"
static meow_u128
xxHash64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(meow_u64 *)&Result = XXH64(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/xxh3.h"
static meow_u128
xx3Hash64(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(XXH64_hash_t *)&Result = XXH3_64bits_withSeed(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

static meow_u128
xx3Hash128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(XXH128_hash_t *)&Result = XXH3_128bits_withSeed(Source, Len, *(meow_u64 *)Seed128);
    
    return(Result);
}

#include "other/clhash.c"
static void *CLHashJunk;
static meow_u128
CLHash64(void *Seed128, meow_u64 Len, void *Source)
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
HighwayHash128(void *Seed128, meow_u64 Len, void *Source)
{
    highway_result Result;
    
    HighwayHash128((uint8_t *)Source, Len, (uint64_t *)Seed128, Result.Hash);
    
    return(Result.Meow);
}

#include "other/city.h"
#include "other/city.cc"
static meow_u128
CityHash128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result = {};
    
    *(uint128 *)&Result = CityHash128((char *)Source, Len);
    
    return(Result);
}

#define FARMHASH_NO_BUILTIN_EXPECT // NOTE(casey): It appears Farm Hash doesn't support __assume, only __builtin_expect, I guess?
#include "other/farmhash.cc"
static meow_u128
FarmHash128(void *Seed128, meow_u64 Len, void *Source)
{
    meow_u128 Result;
    
    /* NOTE(casey): I know it LOOKS like this is calling CityHash, but it's not - Farm's 128-bit hash is defined like this:
    
       return DebugTweak(farmhashcc::CityHash128WithSeed(s, len, seed));
       
       And then inside their CityHash128WithSeed, they call Murmur on small values, etc.  So this just goes more directly
       to the source (hopefully - oh my god Farm Hash's code is giant)
    */
    
    util::uint128_t R = util::farmhashcc::CityHash128WithSeed((const char *)Source, (size_t)Len, util::Uint128(((meow_u64 *)Seed128)[0], ((meow_u64 *)Seed128)[1]));
    
    Result = *(meow_u128 *)&R;
    return(Result);
}

#endif

//
// NOTE(casey): List of available hash implementations
//

typedef meow_u128 meow_hash_implementation(void *Seed, meow_u64 Len, void *Source);

typedef void meow_begin_implementation(void *State, void *Seed128);
typedef void meow_absorb_implementation(void *State, meow_u64 Len, void *Source);
typedef meow_u128 meow_end_implementation(void *State, meow_u8 *Store128);

struct named_hash_type
{
    char *ShortName;
    char *FullName;
    
    meow_hash_implementation *Imp;
    meow_hash_implementation *Reference;
    
    meow_begin_implementation *Begin;
    meow_absorb_implementation *Absorb;
    meow_end_implementation *End;
};

#if MEOW_INCLUDE_ASM
extern "C" meow_u128 MeowHash_ASM(void *Seed128Init, meow_umm Len, void *SourceInit);
#endif

static named_hash_type NamedHashTypes[] =
{
#define MEOW_HASH_TEST_INDEX_128 0
    
    {(char *)"Meow128", (char *)"Meow 128-bit AES-NI", MeowHash, MeowHash, (meow_begin_implementation *)MeowBegin, (meow_absorb_implementation *)MeowAbsorb, (meow_end_implementation *)MeowEnd},
    
#if MEOW_INCLUDE_ASM
    {(char *)"Meow128A", (char *)"Meow 128-bit V-ASM", MeowHash_ASM, MeowHash},
#endif
    
#if MEOW_INCLUDE_TRUNCATIONS
    {(char *)"Meow64", (char *)"Meow 64-bit AES-NI", MeowHashTruncate64},
    {(char *)"Meow32", (char *)"Meow 32-bit AES-NI", MeowHashTruncate32},
#endif
    
#if MEOW_INCLUDE_OTHER_HASHES
    {(char *)"wyhash", (char *)"wyhash 64-bit", Wyhash64},
    {(char *)"Falk128", (char *)"Falk Hash 128-bit", FalkHash128},
    {(char *)"CL", (char *)"CLHash 64-bit", CLHash64},
    {(char *)"t1ha64", (char *)"t1ha 64-bit", t1ha64},
    {(char *)"xx3128", (char *)"xxHash3 128-bit", xx3Hash128},
    {(char *)"xx364", (char *)"xxHash3 64-bit", xx3Hash64},
    {(char *)"Met128", (char *)"Metro Hash 128-bit", MetroHash128},
    {(char *)"City128", (char *)"City Hash 128-bit", CityHash128},
    {(char *)"Farm", (char *)"Farm Hash 128-bit", FarmHash128},
    {(char *)"spooky", (char *)"Spooky Hash v2 128-bit", Spooky128},
    {(char *)"xx64", (char *)"xxHash 64-bit", xxHash64},
    {(char *)"mum64", (char *)"mum 64-bit", Mum64},
    
    // NOTE(casey): Highway Hash is disabled until someone provides a usable ~4 file implementation
    // that is optimized.
    //    {(char *)"High128", (char *)"Highway Hash 128-bit", HighwayHash128},
#endif
};

static void
PrintSize(FILE *Stream, double Size, int Fixed, int AllowDecimals = 1)
{
    int Decimals = 0;
    
    // NOTE(casey): Because this is for _printing_, we have to knock the size down
    // every time it is higher than 1000, no 1024, to avoid printing 4 characters.
    
    char *Suffix = Fixed ? (char *)"b " : (char *)"b";
    if(Size >= 1000.0)
    {
        Decimals = AllowDecimals;
        
        Suffix = (char *)"kb";
        Size /= 1024.0;
        if(Size >= 1000.0)
        {
            Suffix = (char *)"mb";
            Size /= 1024.0;
            if(Size >= 1000.0)
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
    fprintf(Stream, "%08X-%08X-%08X-%08X",
            MeowU32From(Hash, 3),
            MeowU32From(Hash, 2),
            MeowU32From(Hash, 1),
            MeowU32From(Hash, 0));
}

static void
InitializeHashesThatNeedInitializers(void)
{
#if MEOW_INCLUDE_OTHER_HASHES
    CLHashJunk = get_random_key_for_clhash(1234, 5678);
#endif
}
