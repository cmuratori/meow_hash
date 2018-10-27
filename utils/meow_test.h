/* ========================================================================

   meow_test.h - shared functions for Meow testing utilities
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#if _MSC_VER
#include <intrin.h>
#define TRY __try
#define CATCH __except(1)
#define malloc(a) _aligned_malloc(4,a)
#define aligned_alloc(a,b) _aligned_malloc(b,a)
#define free _aligned_free
#else
#include <x86intrin.h>
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
#endif


#include "meow_hash.h"

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

static meow_hash
MeowHashTruncate64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = MeowHash1(Seed, Len, Source);
    Result.u64[1] = 0;
    return(Result);
}

static meow_hash
MeowHashTruncate32(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = MeowHashTruncate64(Seed, Len, Source);
    Result.u32[1] = 0;
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

#if !defined(_MSC_VER) || (_MSC_FULL_VER >= 191025017)
#define MEOW_T1HA_INCLUDED 1
#else
#define MEOW_T1HA_INCLUDED 0
#endif

#include "other/city.h"
#include "other/city.cc"
static meow_hash
CityHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = {};
    
    uint128 Temp = CityHash128((char *)Source, Len);
    Result.u64[0] = Temp.first;
    Result.u64[1] = Temp.second;
    
    return(Result);
}

#include "other/falkhash.c"
static meow_hash
FalkHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = {};
    
    Result.u128 = falkhash(Source, Len, Seed);
    
    return(Result);
}

#include "other/metrohash128.h"
#include "other/metrohash128.cpp"
static meow_hash
MetroHash128(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = {};
    
    MetroHash128::Hash((uint8_t *)Source, Len, (uint8_t *)&Result, Seed);
    
    return(Result);
}

#if MEOW_T1HA_INCLUDED
#include "other/t1ha0_ia32aes_avx.c"
static meow_hash
t1ha64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = {};
    
    Result.u64[0] = t1ha0_ia32aes_avx(Source, Len, Seed);
    
    return(Result);
}
#endif

#include "other/xxhash.c"
static meow_hash
xxHash64(meow_u64 Seed, meow_u64 Len, void *Source)
{
    meow_hash Result = {};
    
    Result.u64[0] = XXH64(Source, Len, Seed);
    
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
    meow_macroblock_op *Op;
};

static named_hash_type NamedHashTypes[] =
{
#define MEOW_HASH_TEST_INDEX_128 0
    {(char *)"Meow128", (char *)"Meow 128-bit AES-NI 128-wide", MeowHash1, MeowHash1Op},
#if MEOW_HASH_AVX512
    {(char *)"Meow128x2", (char *)"Meow 128-bit VAES 256-wide", MeowHash2, MeowHash2Op},
    {(char *)"Meow128x4", (char *)"Meow 128-bit VAES 512-wide", MeowHash4, MeowHash4Op},
#endif
#if MEOW_INCLUDE_TRUNCATIONS
    {(char *)"Meow64", (char *)"Meow 64-bit AES-NI 128-wide", MeowHashTruncate64},
    {(char *)"Meow32", (char *)"Meow 32-bit AES-NI 128-wide", MeowHashTruncate32},
#endif
    
#if MEOW_INCLUDE_OTHER_HASHES
#if MEOW_T1HA_INCLUDED
    {(char *)"t1ha64", (char *)"t1ha 64-bit", t1ha64},
#endif
    {(char *)"Falk128", (char *)"Falk Hash 128-bit", FalkHash128},
    {(char *)"xx64", (char *)"xxHash 64-bit", xxHash64},
    {(char *)"Met128", (char *)"Metro Hash 128-bit", MetroHash128},
    {(char *)"City128", (char *)"City Hash 128-bit", CityHash128},
#endif
};

static void
PrintSize(FILE *Stream, double Size, int Fixed)
{
    char *Suffix = Fixed ? (char *)"b " : (char *)"b";
    if(Size >= 1024.0)
    {
        Suffix = (char *)"kb";
        Size /= 1024.0;
        if(Size >= 1024.0)
        {
            Suffix = (char *)"mb";
            Size /= 1024.0;
            if(Size >= 1024.0)
            {
                Suffix = (char *)"gb";
                Size /= 1024.0;
            }
        }
    }
    
    fprintf(Stream, Fixed ? "%4.0f%s" : "%0.0f%s", Size, Suffix);
}

static void
PrintHash(FILE *Stream, meow_hash Hash)
{
    fprintf(Stream, "%08X-%08X-%08X-%08X", Hash.u32[3], Hash.u32[2], Hash.u32[1], Hash.u32[0]);
}
