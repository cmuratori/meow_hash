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
   
   For a complete working example, see meow_example.cpp.
   
   To hash a block of data, call a MeowHash implementation:
   
       #include <intrin.h>
       #include "meow_hash.h"
       
       // Always available
       meow_lane MeowHash1(u64 Seed, u64 Len, void *Source);
       
       // Available only if you #define MEOW_HASH_256
       meow_lane MeowHash2(u64 Seed, u64 Len, void *Source);
       
       // Available only if you #define MEOW_HASH_512
       meow_lane MeowHash4(u64 Seed, u64 Len, void *Source);
       
   MeowHash1 is 128-bit wide AES-NI.  MeowHash2 is 256-bit wide VAES.
   MeowHash4 is 512-bit wide VAES.  As of the initial publication of
   Meow hash, no consumer CPUs exist which support VAES, so these
   are for future use and internal x64 vendor testing.
   
   Calling MeowHash* with a seed, length, and source pointer invokes the
   hash and returns a meow_lane union which contains the 512-bit result
   accessible in a number of ways (u64[8], _m128i[4], _m256i[2],
   and _m512i).  From there you can pull out what you want and discard the
   rest, as the Meow hash is designed to produce high-quality hashes
   when truncated down to anything 32 bits or greater.
   
   Since no currently available CPUs can run MeowHash2 or MeowHash4,
   it is not recommended that you include them in your code, because
   they literally _cannot_ be tested.  Once CPUs are available that
   can run them, you can include them and use a probing function
   to see if they can be used at startup, as shown in meow_example.cpp.
   
   ======================================================================== */

#if defined(MEOW_HASH_SPECIALIZED)

static meow_lane
MEOW_HASH_SPECIALIZED(meow_u64 Seed, meow_u64 Len, void *SourceInit)
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
    meow_u8 *Source = (meow_u8 *)SourceInit;
    meow_u64 BlockCount = (Len >> 8);
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
        meow_u8 *Dest = (meow_u8 *)Partial;
        while(Len--)
        {
            *Dest++ = *Source++;
        }

        meow_u8 *P = (meow_u8 *)Partial;
        AESMerge(S0123, Partial[0]);
        AESMerge(S4567, Partial[1]);
        AESMerge(S89AB, Partial[2]);
        AESMerge(SCDEF, Partial[3]);
    }
    
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

#if !defined(MEOW_HASH_TYPES)
#define MEOW_HASH_VERSION 1
#define MEOW_HASH_VERSION_NAME "0.1 Alpha"
#define meow_u8 char unsigned
#define meow_u32 int unsigned
#define meow_u64 long long unsigned

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
    
    meow_u64 Sub[8];
    meow_u32 Sub32[16];
};

typedef meow_lane meow_hash_implementation(meow_u64 Seed, meow_u64 Len, void *SourceInit);

#define MEOW_HASH_TYPES
#endif

static void
MeowAESRotate128x4(meow_lane &A, meow_lane &B)
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
#define AESRotate MeowAESRotate128x4
#include "meow_hash.h"

// NOTE(casey): 256-wide VAES Meow (maximum of 32 bytes/clock)
#if defined(MEOW_HASH_256)
static void
MeowAESRotate256x2(meow_lane &A, meow_lane &B)
{
    A.D0 = _mm256_aesdec_epi128(A.D0, B.D0);
    A.D1 = _mm256_aesdec_epi128(A.D1, B.D1);

    // TODO(casey): This can be done with permutation instructions,
    // but I will forgo implementing it that way until I have an
    // actual CPU to test it on!
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
#define AESRotate MeowAESRotate256x2
#include "meow_hash.h"
#endif

// NOTE(casey): 512-wide VAES Meow (maximum of 64 bytes/clock)
#if defined(MEOW_HASH_512)
static void
MeowAESRotate512(meow_lane &A, meow_lane &B)
{
    A.Q0 = _mm512_aesdec_epi128(A.Q0, B.Q0);

    // TODO(casey): This rotate can be done with permutation instructions,
    // but I will forgo implementing it that way until I have an
    // actual CPU to test it on!
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
#define AESRotate MeowAESRotate512
#include "meow_hash.h"
#endif

#endif