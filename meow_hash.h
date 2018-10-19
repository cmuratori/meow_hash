/* ========================================================================

   Meow - A Fast Non-cryptographic Hash for Large Data Sizes
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ========================================================================
   
   LICENSE
   
   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.
   
   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:
   
   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgement in the product documentation would be
      appreciated but is not required.
      
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
      
   3. This notice may not be removed or altered from any source distribution.
   
   ========================================================================
   
   FAQ
   
   Q: What is it?
   A: Meow is a 512-bit non-cryptographic hash that operates at high speeds
      on x64 processors.  It is designed to be truncatable to 256, 128, 64,
      and 32-bit hash values and still retain good collision resistance.
      
   Q: What is it GOOD for?
   A: Quickly hashing large amounts of data for comparison purposes such as
      block deduplication or file verification.  As of its publication in
      October of 2018, it was the fastest hash in the smhasher suite by
      a factor of 3, but it still passes all smhasher tests and has not
      yet produced any spurious collisions in practical deployment as compared
      to a baseline of SHA-1.  It is also designed to get faster with age:
      it already contains 256-wide and 512-wide hash-equivalent versions
      that can be enabled for potentially 4x faster performance on future
      VAES x64 chips when they are available.
      
   Q: What is it BAD for?
   A: Anything security-related.  It is not designed for security and has
      not be analyzed for security.  It should be assumed that it offers
      no security whatsoever.  It is also not designed for small input
      sizes, so although it _can_ hash 1 byte, 4 bytes, 32 bytes, etc.,
      it will end up wasting a lot of time on padding since its minimum
      block size is 256 bytes.  Generally speaking, if you're not usually
      hashing a kilobyte or more, this is probably not the hash you're
      looking for.
      
   Q: Who wrote it and why?
   A: It was written by Casey Muratori (https://caseymuratori.com) for use
      in processing large-footprint assets for the game 1935
      (https://molly1935.com).  The original system used an SHA-1 hash (which
      is not designed for speed), and so to eliminate hashing bottlenecks
      in the pipeline, the Meow hash was designed to produce equivalent
      quality 256-bit hash values as a drop-in replacement that would take
      a fraction of the CPU time.
      
   Q: Why is it called the "Meow hash"?
   A: It was created while Meow the Infinite (https://meowtheinfinite.com)
      was in development at Molly Rocket, so there were lots of Meow the
      Infinite drawings happening at that time.
      
   Q: How does it work?
   A: It was designed to be the fastest possible hash that produces
      collision-free hash values in practice and passes standard hash
      quality checks.  It uses the built-in AES acceleration provided by
      modern CPUs and computes sixteen hash streams in parallel to avoid
      serial dependency stalls.  The sixteen separate hash results are
      then hashed themselves to produce the final hash value.  While only
      four hash streams would suffice for maximum performance on today's
      machines, hypothetical future chips will likely want sixteen.
      Meow was designed to be future-proof by using sixteen streams up
      front, so in the 2020 time frame when such chips start appearing,
      wider execution of Meow can be enabled without needing to change
      any persistent hash values stored in codebases, databases, etc.
      
   ========================================================================
   
   COMPILATION
   
   To use the Meow hash, #include meow_hash.h in a single CPP file in your
   C++ project.  This will include the entire implementation.  You can
   then define your own thunk calls in that file, and define a header file
   with the API of your choice.
   
   It is NOT recommended to expose the Meow hash API directly to the rest
   of your program, because it must return the hash result as a
   non-C-standard type (since the values are SSE/AVX-512/etc.), and your
   project will likely have its own types defined for these.  So at the
   very least, you will want to make a thunk call that wraps the Meow hash
   with a conversion to your preferred 128/256/512-bit type.
   
   By default, only the 128-bit wide, AES-NI version of the hash will
   be compiled, because as of this publication, only the very latest
   versions of most compilers can compile VAES code.  To enable the VAES
   versions, you must use #defines:
   
       #define MEOW_HASH_256 // Enables 256-wide VAES version (MeowHash2)
       #define MEOW_HASH_512 // Enables 512-wide VAES version (MeowHash4)
       #include "meow_hash.h"
       
   ========================================================================
   
   USAGE
   
   To hash a block of data, call MeowHash:
   
       meow_lane MeowHash(u64 Seed, u64 Len, void *Source);
       
   Calling MeowHash with a seed, length, and source pointer invokes the
   hash and returns a meow_lane union which contains the 512-bit result
   accessible in a number of ways (u64[8], _m128i[4], _m256i[2],
   and _m512i).  From there you can pull out what you want and discard the
   rest, as the Meow hash is designed to produce high-quality hashes
   when truncated down to anything 32 bits or greater.
   
   MeowHash is actually a variable which initially points to the most-
   compatible MeowHash routine.  This is also the only routine that
   can be run on today's silicon.  In the future, x64 CPUs will potentially
   support wider AES operations, and Meow has implementations to accelerate
   it on those CPUs.  You can compile them in using #define's (see the
   above section on COMPILATION), and once you have, on initialization
   your program can ask MeowHash to be specialized to the fastest Meow
   by calling:
   
      u32 MeowHashSpecializeForCPU(void);
      
   It will return the bit width it chose (128, 256, or 512).  Not that
   it will only pick from the compiled-in versions, so you must #define
   MEOW_HASH_256 or MEOW_HASH_512 (or both) in order for it to have
   any non-128-bit versions to choose from.
   
   Because it must use a static variable to do this, if you are calling
   MeowHash from multiple threads, you will want to make single call
   to MeowHash at the start of your program to ensure that the detection
   is done cleanly on one thread and the variable is set.  This avoids
   any race conditions that might occur on initialization.
   
   If you would rather do your own processor testing and specialization,
   you can call Meow's processor-specific hashes directly.  They are:
   
       // Always available
       meow_lane MeowHash1(u64 Seed, u64 Len, void *Source);
       
       // Available only if you #define MEOW_HASH_256
       meow_lane MeowHash2(u64 Seed, u64 Len, void *Source);
       
       // Available only if you #define MEOW_HASH_512
       meow_lane MeowHash4(u64 Seed, u64 Len, void *Source);
       
   MeowHash1 is 128-bit wide AES-NI.  MeowHash2 is 256-bit wide VAES.
   MeowHash4 is 512-bit wide VAES.  As of the initial publication of
   Meow hash, no consumer CPUs exist which support VAES, so really this
   is all for future use.
   
   In addition to the MeowHash calls, there are also thunks included
   that comply with the standard smhasher function prototype, since that
   is the most common non-cryptographic hash testing framework.
   There are versions for 32, 64, 128, and 256-bit hashing:
   
       void Meow_AES_32(const void * key, int len, u32 seed, void * out);
       void Meow_AES_64(const void * key, int len, u32 seed, void * out);
       void Meow_AES_128(const void * key, int len, u32 seed, void * out);
       void Meow_AES_256(const void * key, int len, u32 seed, void * out);
       
   These can be dropped into the smhasher hash listing table for testing.
   
   There is no 512-bit thunk because smhasher doesn't support 512-bit
   hashes.
   
   ======================================================================== */

#if defined(MEOW_HASH_SPECIALIZED)

static meow_lane
MEOW_HASH_SPECIALIZED(u64 Seed, u64 Len, void *SourceInit)
{
    // NOTE(casey): The initialization vector follows falkhash's lead and uses the seed twice, but the second time
    // the length plus one is added to differentiate.  This seemed sensible, but I haven't thought too hard about this,
    // there may be better things to use as an IV.
    meow_lane IV;
    IV.Sub[0] = IV.Sub[2] = IV.Sub[4] = IV.Sub[6] = Seed;
    IV.Sub[1] = IV.Sub[3] = IV.Sub[5] = IV.Sub[7] = Seed + Len + 1;
    
    // NOTE(casey): Initialize all 16 streams with the initialization vector
    meow_lane S0123 = IV;
    meow_lane S4567 = IV;
    meow_lane S89AB = IV;
    meow_lane SCDEF = IV;
    
    // NOTE(casey): Handle as many full 256-byte blocks as possible
    u8 *Source = (u8 *)SourceInit;
    u64 BlockCount = (Len >> 8);
    Len -= (BlockCount << 8);
    while(BlockCount--)
    {
        AESLoad(S0123, Source);
        AESLoad(S4567, Source + 64);
        AESLoad(S89AB, Source + 128);
        AESLoad(SCDEF, Source + 192);
        Source += (1 << 8);
    }
    
    // NOTE(casey): If residual data remains, hash one final 256-byte block padded with the initialization vector
    if(Len)
    {
        // TODO(casey): Can this just be zeroes?
        meow_lane Partial[] = {IV, IV, IV, IV};
        u8 *Dest = (u8 *)Partial;
        while(Len--)
        {
            *Dest++ = *Source++;
        }

        u8 *P = (u8 *)Partial;
        AESMerge(S0123, Partial[0]);
        AESMerge(S4567, Partial[1]);
        AESMerge(S89AB, Partial[2]);
        AESMerge(SCDEF, Partial[3]);
    }
    
    //
    // NOTE(casey): Special thanks here to Fabian Giesen (https://fgiesen.wordpress.com/)
    // for discussions about how to best do the final merge!
    //
    
    // NOTE(casey): Combine the 16 streams into a single hash to spread the bits out evenly
    meow_lane R0 = IV;
    AESRotate(R0, S0123);
    AESRotate(R0, S4567);
    AESRotate(R0, S89AB);
    AESRotate(R0, SCDEF);
    
    AESRotate(R0, S0123);
    AESRotate(R0, S4567);
    AESRotate(R0, S89AB);
    AESRotate(R0, SCDEF);
    
    AESRotate(R0, S0123);
    AESRotate(R0, S4567);
    AESRotate(R0, S89AB);
    AESRotate(R0, SCDEF);
    
    AESRotate(R0, S0123);
    AESRotate(R0, S4567);
    AESRotate(R0, S89AB);
    AESRotate(R0, SCDEF);
    
    // NOTE(casey): Repeat AES enough times to ensure diffusion to all bits in each 128-bit lane
    AESMerge(R0, IV);
    AESMerge(R0, IV);
    AESMerge(R0, IV);
    AESMerge(R0, IV);
    AESMerge(R0, IV);
    
    return(R0);
}

#undef AESLoad
#undef AESMerge
#undef AESRotate
#undef MEOW_HASH_SPECIALIZED

#else

#if !defined(SizeOf)
#define SizeOf sizeof
#endif

#if !defined(Assert)
#include <assert.h>
#define Assert assert
#endif

#if !defined(MEOW_HASH_TYPES)
#define MEOW_HASH_VERSION 0 // NOTE(casey): Version 0 - pre-release!
#include <intrin.h>
typedef char unsigned u8;
typedef int unsigned u32;
typedef long long unsigned u64;

union meow_lane
{
#if defined(MEOW_HASH_512)
    __m512i Q0;
#endif

#if defined(MEOW_HASH_256)
    struct
    {
        __m256i D0;
        __m256i D1;
    };
#endif

    struct
    {
        __m128i L0;
        __m128i L1;
        __m128i L2;
        __m128i L3;
    };

    u64 Sub[8];
};
#define MEOW_HASH_TYPES
#endif

static void
AESRotate128x4(meow_lane &A, meow_lane &B)
{
    A.L0 = _mm_aesdec_si128(A.L0, B.L0);
    A.L1 = _mm_aesdec_si128(A.L1, B.L1);
    A.L2 = _mm_aesdec_si128(A.L2, B.L2);
    A.L3 = _mm_aesdec_si128(A.L3, B.L3);

    __m128i Temp = B.L0;
    B.L0 = B.L1;
    B.L1 = B.L2;
    B.L2 = B.L3;
    B.L3 = Temp;
}

// NOTE(casey): 128-wide AES-NI Meow (maximum of 16 bytes/clock)
#define MEOW_HASH_SPECIALIZED MeowHash1
#define AESLoad(S, From) \
S.L0 = _mm_aesdec_si128(S.L0, *(__m128i *)(From)); \
S.L1 = _mm_aesdec_si128(S.L1, *(__m128i *)(From + 16)); \
S.L2 = _mm_aesdec_si128(S.L2, *(__m128i *)(From + 32)); \
S.L3 = _mm_aesdec_si128(S.L3, *(__m128i *)(From + 48))
#define AESMerge(A, B) \
A.L0 = _mm_aesdec_si128(A.L0, B.L0); \
A.L1 = _mm_aesdec_si128(A.L1, B.L1); \
A.L2 = _mm_aesdec_si128(A.L2, B.L2); \
A.L3 = _mm_aesdec_si128(A.L3, B.L3)
#define AESRotate AESRotate128x4
#include "meow_hash.h"

// NOTE(casey): 256-wide VAES Meow (maximum of 32 bytes/clock)
#if defined(MEOW_HASH_256)
static void
AESRotate256x2(meow_lane &A, meow_lane &B)
{
    A.D0 = _mm256_aesdec_epi128(A.D0, B.D0);
    A.D1 = _mm256_aesdec_epi128(A.D1, B.D1);

    __m128i Temp = B.L0;
    B.L0 = B.L1;
    B.L1 = B.L2;
    B.L2 = B.L3;
    B.L3 = Temp;
}
#define MEOW_HASH_SPECIALIZED MeowHash2
#define AESLoad(S, From) \
S.D0 = _mm256_aesdec_epi128(S.D0, *(__m256i *)(From)); \
S.D1 = _mm256_aesdec_epi128(S.D1, *(__m256i *)(From + 32))
#define AESMerge(A, B) \
A.D0 = _mm256_aesdec_epi128(A.D0, B.D0); \
A.D1 = _mm256_aesdec_epi128(A.D1, B.D1)
#define AESRotate AESRotate256x2
#include "meow_hash.h"
#endif

// NOTE(casey): 512-wide VAES Meow (maximum of 64 bytes/clock)
#if defined(MEOW_HASH_512)
static void
AESRotate512(meow_lane &A, meow_lane &B)
{
    A.Q0 = _mm512_aesdec_epi128(A.Q0, B.Q0);

    __m128i Temp = B.L0;
    B.L0 = B.L1;
    B.L1 = B.L2;
    B.L2 = B.L3;
    B.L3 = Temp;
}
#define MEOW_HASH_SPECIALIZED MeowHash4
#define AESLoad(S, From) \
S.Q0 = _mm512_aesdec_epi128(S.Q0, *(__m512i *)(From))
#define AESMerge(A, B) \
A.Q0 = _mm512_aesdec_epi128(A.Q0, B.Q0);
#define AESRotate AESRotate512
#include "meow_hash.h"
#endif

//
// NOTE(casey): Auto-switching 128/256/512 call
//

typedef meow_lane meow_hash_specialized(u64 Seed, u64 Len, void *SourceInit);
static meow_hash_specialized *MeowHash = MeowHash1;

#if !defined(MEOW_HASH_SPECIALIZE_FOR_CPU)
u32 MeowHashSpecializeForCPU(void)
{
    u32 Result = 0;
    
#if defined(MEOW_HASH_512)
    __try
    {
        u8 Garbage[64];
        MeowHash4(0, SizeOf(Garbage), Garbage);
        MeowHash = MeowHash4;
        Result = 512;
    }
    __except(1)
#endif
    {
#if defined(MEOW_HASH_256)
        __try
        {
            u8 Garbage[64];
            MeowHash2(0, SizeOf(Garbage), Garbage);
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
#define MEOW_HASH_SPECIALIZE_FOR_CPU 1
#endif

//
// NOTE(casey): smhasher-compatible thunks for testing
//

#if !defined(MEOW_HASH_SMHASHER_THUNKS)
void
Meow_AES_32(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash(seed, len, (void *)key);
    *(u32 *)out = (u32)Result.Sub[0];
}

void
Meow_AES_64(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
}

void
Meow_AES_128(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
}

void
Meow_AES_256(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
    ((u64 *)out)[2] = Result.Sub[2];
    ((u64 *)out)[3] = Result.Sub[3];
}

void
Meow_AES_512(const void * key, int len, u32 seed, void * out)
{
    meow_lane Result = MeowHash(seed, len, (void *)key);
    ((u64 *)out)[0] = Result.Sub[0];
    ((u64 *)out)[1] = Result.Sub[1];
    ((u64 *)out)[2] = Result.Sub[2];
    ((u64 *)out)[3] = Result.Sub[3];
    ((u64 *)out)[4] = Result.Sub[4];
    ((u64 *)out)[5] = Result.Sub[5];
    ((u64 *)out)[6] = Result.Sub[6];
    ((u64 *)out)[7] = Result.Sub[7];
}

#define MEOW_HASH_SMHASHER_THUNKS 1
#endif

#if MEOW_BENCH
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

struct named_specialization
{
    char *Name;
    meow_hash_specialized *Handler;
};
void
main(int ArgCount, char **Args)
{
    named_specialization Specializations[] =
    {
#if defined(MEOW_HASH_512)
        {"MeowHash AVX-512 VAES", MeowHash4},
#endif
#if defined(MEOW_HASH_256)
        {"MeowHash AVX-256 VAES", MeowHash2},
#endif
        {"MeowHash SSE AES-NI  ", MeowHash1},
    };
    
    printf("\n");
    printf("MeowBench %u - speed self-test for the Meow hash\n", MEOW_HASH_VERSION);
    printf("    See https://mollyrocket.com/meowhash for details\n");
    printf("    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    printf("\n");
    printf("Versions compiled into this benchmark:\n");
    for(u32 SpecializationIndex = 0;
        SpecializationIndex < (sizeof(Specializations)/sizeof(Specializations[0]));
        ++SpecializationIndex)
    {
        named_specialization Specialization = Specializations[SpecializationIndex];
        printf("    %d. %s\n", SpecializationIndex + 1, Specialization.Name);
    }
    
    printf("\n");
    printf("Test results:\n");
    u32 Size = 32*1024;
    u32 TestCount = 8000000;
    for(u32 Batch = 0;
        Batch < 16;
        ++Batch)
    {
        void *Buffer = _aligned_malloc(Size, 128);
        
        for(u32 SpecializationIndex = 0;
            SpecializationIndex < (sizeof(Specializations)/sizeof(Specializations[0]));
            ++SpecializationIndex)
        {
            named_specialization Specialization = Specializations[SpecializationIndex];
            __try
            {
                u64 BestClocks = (u64)-1;
                for(u32 Test = 0;
                    Test < TestCount;
                    ++Test)
                {
                    u64 StartClock = __rdtsc();
                    Specialization.Handler(0, Size, Buffer);
                    u64 EndClock = __rdtsc();
                    
                    u64 Clocks = EndClock - StartClock;
                    if(BestClocks > Clocks)
                    {
                        printf("\r%s least cycles to hash %uk: %u (%f bytes/cycle)               ",
                               Specialization.Name,
                               (Size/1024), (u32)BestClocks, (double)Size / (double)BestClocks);
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
#endif

#endif