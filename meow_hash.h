/* ========================================================================

   Meow - A Fast Non-cryptographic Hash for Large Data Sizes
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ========================================================================
   
   zlib License
   
   (C) Copyright 2018 Molly Rocket, Inc.
   
   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.
   
   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:
   
   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.
   
   ========================================================================
   
   FAQ
   
   Q: What is it?
   A: Meow is a 128-bit non-cryptographic hash that operates at high speeds
      on x64 processors.  It is designed to be truncatable to 256, 128, 64,
      and 32-bit hash values and still retain good collision resistance.
      
   Q: What is it GOOD for?
   A: Quickly hashing large amounts of data for comparison purposes such as
      block deduplication or file verification.  As of its publication in
      October of 2018, it was the fastest hash in the smhasher suite by
      a factor of 3, but it still passes all smhasher tests and has not
      yet produced any spurious collisions in practical deployment as
      compared to a baseline of SHA-1.  It is also designed to get faster
      with age: it already contains 256-wide and 512-wide hash-equivalent
      versions that can be enabled for potentially 4x faster performance
      on future VAES x64 chips when they are available.
      
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
      (https://molly1935.com).  The original system used an SHA-1 hash
      (which is not designed for speed), and so to eliminate hashing
      bottlenecks in the pipeline, the Meow hash was designed to produce
      equivalent quality 256-bit hash values as a drop-in replacement that
      would take a fraction of the CPU time.
      
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
   
   If your compiler is older, and has trouble compiling code for AVX-512,
   you can turn off the AVX-512 paths in Meow hash with a #define:
   
       #define MEOW_HASH_AVX512 0
       
   You can place this before you include meow_hash.h, or you can put
   it in the compiler options for your compiler to define it automatically.
   
   If you need Meow to use different types than the default for u8, u32,
   u64, and
   
   **** VERY IMPORTANT COMPILATION NOTE ****
   
   Meow uses the AESDEC instruction, which comes in two flavors:
   SSE (aesdec) and AVX (vaesdec).  If you are compiling _with_ AVX support,
   your compiler will probably emit the AVX variant, which means your code
   WILL NOT RUN on computers that do not have AVX.  If you need to deploy
   this hash on computers that do not have AVX, you must take care to
   TURN OFF support for AVX in your compiler for the file that includes
   the Meow hash!
   
   ========================================================================
   
   USAGE
   
   For a complete working example, see meow_example.cpp.
   
   In order to use the Meow hash, you must have x64 intrinsics defined.
   This requires including a compiler-specific intrinsics file, such as:
   
       #ifdef _MSC_VER
       #include <intrin.h>
       #else
       #include <x86intrin.h>
       #endif
       
   Once you have intrinsics defined, to hash a block of data,
   call a MeowHash implementation:
   
       #include "meow_hash.h"
       
       // NOTE(casey): Source MUST be aligned to MEOW_HASH_ALIGNMENT
       // if you are not compiling for platforms with AVX support!
       
       // Always available
       meow_hash MeowHash1(u64 Seed, u64 Len, void *Source);
       
       // Available only when compiling with AVX extensions
       meow_hash MeowHash2(u64 Seed, u64 Len, void *Source);
       meow_hash MeowHash4(u64 Seed, u64 Len, void *Source);
       
   MeowHash1 is 128-bit wide AES-NI.  MeowHash2 is 256-bit wide VAES.
   MeowHash4 is 512-bit wide VAES.  As of the initial publication of
   Meow hash, no consumer CPUs exist which support VAES, so these
   are for future use and internal x64 vendor testing.
   
   Calling MeowHash* with a seed, length, and source pointer invokes the
   hash and returns a meow_hash union which contains the 128-bit result
   accessible in a number of ways (u32[4], u64[2], _m128i).  From there
   you can pull out what you want and discard the rest, as the Meow hash
   is designed to produce high-quality hashes when truncated down to
   anything 32 bits or greater.
 
   Since no currently available CPUs can run MeowHash2 or MeowHash4,
   it is not recommended that you include them in your code, because
   they literally _cannot_ be tested.  Once CPUs are available that
   can run them, you can include them and use a probing function
   to see if they can be used at startup, as shown in meow_example.cpp.
   
   **** VERY IMPORTANT PROGRAMMING NOTE ****
   
   If you are compiling with AES-NI (not AVX), the source pointer to
   MeowHash1 _must_ be aligned to the byte boundary specified by the
   #define MEOW_HASH_ALIGNMENT, or Meow hash may fault with an
   unaligned load.
   
   Because MeowHash2 and MeowHash4 are AVX-512 only, they do not have
   these limitations, because they _cannot_ be compiled in non-AVX
   modes.  So if you can run MeowHash2 and MeowHash4 at all, you can
   run them on unaligned buffers just fine.
   
   TL;DR: If you don't know what AES-NI, VAES, VEX, etc. are, then just
   always align your buffers :)  It's not hard to do, and it will
   avoid you accidentally shipping code that crashes with unaligned
   load exceptions.
   
   ======================================================================== */

//
// NOTE(casey): This version is EXPERIMENTAL.  The Meow hash is still
// undergoing testing and finalization.
//
// **** EXPECT HASHES/APIs TO CHANGE UNTIL THE VERSION NUMBER HITS 1.0. ****
//
// You have been warned
//

//
// NOTE(casey): There is a lot of manually expanded stuff in this code.
// It's not because I like typing things ad infinitum.  It's because compilers
// still aren't good enough to get the machine code right if you start
// adding layers of indirection.  Believe me, I like copying the same line
// 16 times and typing S0 through SF about as much as you do, but at the
// moment, if you want the code to be fast across many different compilers,
// it can't be helped.
//

#if !defined(MEOW_HASH_AVX512)
#define MEOW_HASH_AVX512 0
#endif

#if !defined(MEOW_HASH_TYPES)
#define meow_u8 char unsigned
#define meow_u16 short unsigned
#define meow_u32 int unsigned
#define meow_u64 long long unsigned
#define meow_u128 __m128i
#if MEOW_HASH_AVX512
#define meow_u256 __m256i
#define meow_u512 __m512i
#endif
#define MEOW_HASH_TYPES

#define Meow128_AreEqual(A, B) (_mm_movemask_epi8(_mm_cmpeq_epi8((A), (B))) == 0xFFFF)
#define Meow128_AESDEC(Prior, XOr) _mm_aesdec_si128((Prior), (XOr))
#define Meow128_AESDEC_Mem(Prior, XOrPtr) _mm_aesdec_si128((Prior), *(meow_u128 *)(XOrPtr))
#define Meow128_Set64x2(Low64, High64) _mm_set_epi64x((High64), (Low64))
#define Meow128_LoadUnaligned(Ptr) _mm_loadu_si128((meow_u128 *)(Ptr))

// TODO(casey): Not sure if this should actually be Meow128_Zero(A) ((A) = _mm_setzero_si128()), maybe
#define Meow128_Zero() _mm_setzero_si128()

#define Meow256_AESDEC(Prior, XOr) _mm256_aesdec_epi128((Prior), (XOr))
#define Meow256_AESDEC_Mem(Prior, XOrPtr) _mm256_aesdec_epi128((Prior), *(meow_u256 *)(XOrPtr))
#define Meow256_Store(Value, Ptr) _mm256_store_si256((meow_u256 *)(Ptr), (Value));
#define Meow256_Zero() _mm256_setzero_si256()

#define Meow512_AESDEC(Prior, XOr) _mm512_aesdec_epi128((Prior), (XOr))
#define Meow512_AESDEC_Mem(Prior, XOrPtr) _mm512_aesdec_epi128((Prior), *(meow_u256 *)(XOrPtr))
#define Meow512_Store(Value, Ptr) _mm256_store_si256((meow_u512 *)(Ptr), (Value));
#define Meow512_Zero() _mm512_setzero_si512()

#endif

#define MEOW_HASH_VERSION 3
#define MEOW_HASH_VERSION_NAME "0.3/snowshoe"
#define MEOW_HASH_ALIGNMENT 128
#define MEOW_HASH_MACROBLOCK_COUNT 4096
#define MEOW_HASH_MACROBLOCK_SIZE (MEOW_HASH_MACROBLOCK_COUNT << MEOW_HASH_BLOCK_SIZE_SHIFT)
#define MEOW_HASH_BLOCK_SIZE_SHIFT 8

#if MEOW_HASH_IACA
// NOTE(casey): Define this if you'd like to analyze Meow hash with IACA
#include <iacaMarks.h>
#define MEOW_ANALYSIS_START IACA_VC64_START
#define MEOW_ANALYSIS_END IACA_VC64_END
#else
#define MEOW_ANALYSIS_START
#define MEOW_ANALYSIS_END
#endif

typedef struct meow_source_blocks
{
    meow_u64 TotalLengthInBytes;
    meow_u64 BlockCount;
    meow_u64 MacroblockCount;
    meow_u8 *Source;
    meow_u8 *OverhangStart;

    int Overhang;
} meow_source_blocks;

typedef struct meow_macroblock
{
    meow_u8 *Source;
    int BlockCount;
} meow_macroblock;

typedef struct meow_macroblock_result
{
    meow_u128 S0;
    meow_u128 S1;
    meow_u128 S2;
    meow_u128 S3;
    meow_u128 S4;
    meow_u128 S5;
    meow_u128 S6;
    meow_u128 S7;
    meow_u128 S8;
    meow_u128 S9;
    meow_u128 SA;
    meow_u128 SB;
    meow_u128 SC;
    meow_u128 SD;
    meow_u128 SE;
    meow_u128 SF;
} meow_macroblock_result;

typedef meow_macroblock_result meow_macroblock_op(int BlockCount, meow_u8 *Source);
typedef meow_u128 meow_hash_implementation(meow_u64 Seed, meow_u64 Len, void *SourceInit);

//
// NOTE(casey): Smaller hash extraction
//
static meow_u64
MeowU64From(meow_u128 Hash)
{
    // TODO(casey): It is probably worth it to use the cvt intrinsics here
    meow_u64 Result = *(meow_u64 *)&Hash;
    return(Result);
}

static meow_u32
MeowU32From(meow_u128 Hash)
{
    // TODO(casey): It is probably worth it to use the cvt intrinsics here
    meow_u32 Result = *(meow_u32 *)&Hash;
    return(Result);
}

//
// NOTE(casey): "Fast" comparison (using SSE)
//
static int
MeowHashesAreEqual(meow_u128 A, meow_u128 B)
{
    int Result = Meow128_AreEqual(A, B);
    return(Result);
}

//
// NOTE(casey): Macro-block based hashing, for multithreading very large hashes,
// due to prodding by speed demon Jeff Roberts of RAD Game Tools, Inc. (https://radgametools.com)
//

static meow_source_blocks
MeowSourceBlocksFor(meow_u64 TotalLengthInBytes, void *Source)
{
    meow_source_blocks Result;
    
    Result.TotalLengthInBytes = TotalLengthInBytes;
    Result.BlockCount = (TotalLengthInBytes >> MEOW_HASH_BLOCK_SIZE_SHIFT);
    Result.MacroblockCount = (Result.BlockCount + MEOW_HASH_MACROBLOCK_COUNT - 1) / MEOW_HASH_MACROBLOCK_COUNT;
    Result.Source = (meow_u8 *)Source;
    
    Result.Overhang = (int)(TotalLengthInBytes - (Result.BlockCount << MEOW_HASH_BLOCK_SIZE_SHIFT));
    Result.OverhangStart = Result.Source + (Result.TotalLengthInBytes - Result.Overhang);
    
    return(Result);
}

static meow_macroblock
MeowGetMacroblock(meow_source_blocks volatile *Blocks, meow_u64 Index)
{
    meow_u64 Offset = (Index * MEOW_HASH_MACROBLOCK_SIZE);
    meow_u64 BlockCount = ((Blocks->TotalLengthInBytes - Offset) >> MEOW_HASH_BLOCK_SIZE_SHIFT);
    
    meow_macroblock Result;
    Result.Source = Blocks->Source + Offset;
    Result.BlockCount = MEOW_HASH_MACROBLOCK_COUNT;
    if(Result.BlockCount > BlockCount)
    {
        Result.BlockCount = (int)BlockCount;
    }
    
    return(Result);
}

static void
MeowHashMerge(meow_macroblock_result *A, meow_macroblock_result *B)
{
    A->S0 = Meow128_AESDEC(A->S0, B->S0);
    A->S1 = Meow128_AESDEC(A->S1, B->S1);
    A->S2 = Meow128_AESDEC(A->S2, B->S2);
    A->S3 = Meow128_AESDEC(A->S3, B->S3);
    A->S4 = Meow128_AESDEC(A->S4, B->S4);
    A->S5 = Meow128_AESDEC(A->S5, B->S5);
    A->S6 = Meow128_AESDEC(A->S6, B->S6);
    A->S7 = Meow128_AESDEC(A->S7, B->S7);
    A->S8 = Meow128_AESDEC(A->S8, B->S8);
    A->S9 = Meow128_AESDEC(A->S9, B->S9);
    A->SA = Meow128_AESDEC(A->SA, B->SA);
    A->SB = Meow128_AESDEC(A->SB, B->SB);
    A->SC = Meow128_AESDEC(A->SC, B->SC);
    A->SD = Meow128_AESDEC(A->SD, B->SD);
    A->SE = Meow128_AESDEC(A->SE, B->SE);
    A->SF = Meow128_AESDEC(A->SF, B->SF);
}

static meow_u128
MeowHashFinish(meow_macroblock_result *State, meow_u64 Seed, meow_u64 TotalLengthInBytes, int Overhang, meow_u8 *Source)
{
    meow_u128 S0 = State->S0;
    meow_u128 S1 = State->S1;
    meow_u128 S2 = State->S2;
    meow_u128 S3 = State->S3;
    meow_u128 S4 = State->S4;
    meow_u128 S5 = State->S5;
    meow_u128 S6 = State->S6;
    meow_u128 S7 = State->S7;
    meow_u128 S8 = State->S8;
    meow_u128 S9 = State->S9;
    meow_u128 SA = State->SA;
    meow_u128 SB = State->SB;
    meow_u128 SC = State->SC;
    meow_u128 SD = State->SD;
    meow_u128 SE = State->SE;
    meow_u128 SF = State->SF;
    
    // NOTE(casey): Handle as many full 128-bit lanes as possible
    switch(Overhang >> 4)
    {
        case 15: SE = Meow128_AESDEC_Mem(SE, Source + 224);
        case 14: SD = Meow128_AESDEC_Mem(SD, Source + 208);
        case 13: SC = Meow128_AESDEC_Mem(SC, Source + 192);
        case 12: SB = Meow128_AESDEC_Mem(SB, Source + 176);
        case 11: SA = Meow128_AESDEC_Mem(SA, Source + 160);
        case 10: S9 = Meow128_AESDEC_Mem(S9, Source + 144);
        case  9: S8 = Meow128_AESDEC_Mem(S8, Source + 128);
        case  8: S7 = Meow128_AESDEC_Mem(S7, Source + 112);
        case  7: S6 = Meow128_AESDEC_Mem(S6, Source + 96);
        case  6: S5 = Meow128_AESDEC_Mem(S5, Source + 80);
        case  5: S4 = Meow128_AESDEC_Mem(S4, Source + 64);
        case  4: S3 = Meow128_AESDEC_Mem(S3, Source + 48);
        case  3: S2 = Meow128_AESDEC_Mem(S2, Source + 32);
        case  2: S1 = Meow128_AESDEC_Mem(S1, Source + 16);
        case  1: S0 = Meow128_AESDEC_Mem(S0, Source);
        default:;
    }

    // NOTE(casey): Handle residual by padding to the nearest 16-byte boundary
    if(Overhang & 0xF)
    {
        Source += (Overhang & 0xF0);
        Overhang &= 0xF;
        
        meow_u128 Partial = Meow128_Zero();
        meow_u8 *Dest = (meow_u8 *)&Partial;
        while(Overhang--)
        {
            *Dest++ = *Source++;
        }

        SF = Meow128_AESDEC(SF, Partial);
    }
    
    // TODO(casey): There needs to be a solid idea behind the mixing vector here.
    // Before Meow v1, we need some definitive analysis of what it should be.
    meow_u128 Mixer = Meow128_Set64x2(Seed - TotalLengthInBytes, Seed + TotalLengthInBytes + 1);
    
    S0 = Meow128_AESDEC(S0, S8);
    S1 = Meow128_AESDEC(S1, S9);
    S2 = Meow128_AESDEC(S2, SA);
    S3 = Meow128_AESDEC(S3, SB);
    S4 = Meow128_AESDEC(S4, SC);
    S5 = Meow128_AESDEC(S5, SD);
    S6 = Meow128_AESDEC(S6, SE);
    S7 = Meow128_AESDEC(S7, SF);
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    S4 = Meow128_AESDEC(S4, Mixer);
    S5 = Meow128_AESDEC(S5, Mixer);
    S6 = Meow128_AESDEC(S6, Mixer);
    S7 = Meow128_AESDEC(S7, Mixer);
    
    S0 = Meow128_AESDEC(S0, S4);
    S1 = Meow128_AESDEC(S1, S5);
    S2 = Meow128_AESDEC(S2, S6);
    S3 = Meow128_AESDEC(S3, S7);
    
    S0 = Meow128_AESDEC(S0, S2);
    S1 = Meow128_AESDEC(S1, S3);
    
    S0 = Meow128_AESDEC(S0, S1);
    
    S0 = Meow128_AESDEC(S0, Mixer);
    
    return(S0);
}

static meow_u128
MeowHashViaOp(meow_macroblock_op *Op, meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    // NOTE(casey): Initialize all streams to zero (probably could do this more efficiently, since
    // most of the time it isn't necessary).
    meow_macroblock_result Group = {0};
    
    // NOTE(casey): Reserve some space for individual macroblock hashes
    meow_macroblock_result SubGroup;
    
    // NOTE(casey): Hash 256-byte blocks, broken up into 1-megabyte macroblocks
    // The 256-byte blocks are to ensure good pipelining on future AVX-512 chips.
    // The 1-megabyte macroblocks are to allow (other, distributed) implementations
    // to hash asynchronously across many cores and assemble at the end.
    int First = 1;
    meow_source_blocks Counts = MeowSourceBlocksFor(TotalLengthInBytes, SourceInit);
    meow_u8 *Source = Counts.Source;
    meow_u64 BlockCount = Counts.BlockCount;
    meow_u64 MacroblockCount = Counts.MacroblockCount;
    while(MacroblockCount--)
    {
        // NOTE(casey): Determine how many blocks are in this macroblock
        int SubBlockCount = MEOW_HASH_MACROBLOCK_COUNT;
        if(SubBlockCount > BlockCount)
        {
            SubBlockCount = (int)BlockCount;
        }
        
        if(First)
        {
            // NOTE(casey): To prevent mixing in extra 0's, we _start_ with the initial group,
            // and then merge the hash from there.  This makes the routine clumsier, but probably
            // makes the hash better, so we grin and bear it.
            Group = Op(SubBlockCount, Source);
            First = 0;
        }
        else
        {
            // NOTE(casey): Hash this macroblock's sub-hash
            SubGroup = Op(SubBlockCount, Source);
            
            // NOTE(casey): Merge it in to the existing hash
            MeowHashMerge(&Group, &SubGroup);
        }
    
        // NOTE(casey): Advance to the next macroblock
        BlockCount -= SubBlockCount;
        Source += (SubBlockCount << MEOW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    // NOTE(casey): Hash any residual data and finalize
    meow_u128 Result = MeowHashFinish(&Group, Seed, Counts.TotalLengthInBytes, Counts.Overhang, Source);
    
    return(Result);
}

static meow_macroblock_result
MeowHashMergeArray(meow_u64 MacroBlockCount, meow_macroblock_result *MacroBlockHashes)
{
    meow_macroblock_result Result;
    
    if(MacroBlockCount > 0)
    {
        Result = MacroBlockHashes[0];
        for(meow_u64 MacroBlockIndex = 1;
            MacroBlockIndex < MacroBlockCount;
            ++MacroBlockIndex)
        {
            MeowHashMerge(&Result, MacroBlockHashes + MacroBlockIndex);
        }
    }
    else
    {
        // NOTE(casey): Do the nullification in two steps to support older compilers (MSVC 2012, etc.)
        meow_macroblock_result NullGroup = {0};
        Result = NullGroup;
    }
    
    return(Result);
}

//
// NOTE(casey): 128-wide AES-NI Meow (maximum of 16 bytes/clock single threaded)
//

static meow_macroblock_result
MeowHash1Op(int BlockCount, meow_u8 *Source)
{
    meow_u128 S0 = Meow128_Zero();
    meow_u128 S1 = Meow128_Zero();
    meow_u128 S2 = Meow128_Zero();
    meow_u128 S3 = Meow128_Zero();
    meow_u128 S4 = Meow128_Zero();
    meow_u128 S5 = Meow128_Zero();
    meow_u128 S6 = Meow128_Zero();
    meow_u128 S7 = Meow128_Zero();
    meow_u128 S8 = Meow128_Zero();
    meow_u128 S9 = Meow128_Zero();
    meow_u128 SA = Meow128_Zero();
    meow_u128 SB = Meow128_Zero();
    meow_u128 SC = Meow128_Zero();
    meow_u128 SD = Meow128_Zero();
    meow_u128 SE = Meow128_Zero();
    meow_u128 SF = Meow128_Zero();
    
    while(BlockCount--)
    {
        S0 = Meow128_AESDEC_Mem(S0, Source);
        S1 = Meow128_AESDEC_Mem(S1, Source + 16);
        S2 = Meow128_AESDEC_Mem(S2, Source + 32);
        S3 = Meow128_AESDEC_Mem(S3, Source + 48);
        S4 = Meow128_AESDEC_Mem(S4, Source + 64);
        S5 = Meow128_AESDEC_Mem(S5, Source + 80);
        S6 = Meow128_AESDEC_Mem(S6, Source + 96);
        S7 = Meow128_AESDEC_Mem(S7, Source + 112);
        S8 = Meow128_AESDEC_Mem(S8, Source + 128);
        S9 = Meow128_AESDEC_Mem(S9, Source + 144);
        SA = Meow128_AESDEC_Mem(SA, Source + 160);
        SB = Meow128_AESDEC_Mem(SB, Source + 176);
        SC = Meow128_AESDEC_Mem(SC, Source + 192);
        SD = Meow128_AESDEC_Mem(SD, Source + 208);
        SE = Meow128_AESDEC_Mem(SE, Source + 224);
        SF = Meow128_AESDEC_Mem(SF, Source + 240);
        
        Source += (1 << MEOW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    meow_macroblock_result Result;
    Result.S0 = S0;
    Result.S1 = S1;
    Result.S2 = S2;
    Result.S3 = S3;
    Result.S4 = S4;
    Result.S5 = S5;
    Result.S6 = S6;
    Result.S7 = S7;
    Result.S8 = S8;
    Result.S9 = S9;
    Result.SA = SA;
    Result.SB = SB;
    Result.SC = SC;
    Result.SD = SD;
    Result.SE = SE;
    Result.SF = SF;
    
    return(Result);
}

static meow_u128
MeowHash1(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    //
    // NOTE(casey): For less than 16 bytes, we punt, because we can't guarantee we won't issue a bad load.
    // For more than MEOW_HASH_MACROBLOCK_SIZE, we punt, because we want to support multithreading.
    //
    
    if(TotalLengthInBytes >= MEOW_HASH_MACROBLOCK_SIZE)
    {
        return(MeowHashViaOp(MeowHash1Op, Seed, TotalLengthInBytes, SourceInit));
    }
    
    //
    // NOTE(casey): Initialize all 16 streams to 0
    //
    
    meow_u128 S0 = Meow128_Zero();
    meow_u128 S1 = Meow128_Zero();
    meow_u128 S2 = Meow128_Zero();
    meow_u128 S3 = Meow128_Zero();
    meow_u128 S4 = Meow128_Zero();
    meow_u128 S5 = Meow128_Zero();
    meow_u128 S6 = Meow128_Zero();
    meow_u128 S7 = Meow128_Zero();
    meow_u128 S8 = Meow128_Zero();
    meow_u128 S9 = Meow128_Zero();
    meow_u128 SA = Meow128_Zero();
    meow_u128 SB = Meow128_Zero();
    meow_u128 SC = Meow128_Zero();
    meow_u128 SD = Meow128_Zero();
    meow_u128 SE = Meow128_Zero();
    meow_u128 SF = Meow128_Zero();
    
    //
    // NOTE(casey): Handle as many full 256-byte blocks as possible (16 cycles per block)
    //
    
    meow_u8 *Source = (meow_u8 *)SourceInit;
    int Len = (int)TotalLengthInBytes;
    int BlockCount = (Len >> MEOW_HASH_BLOCK_SIZE_SHIFT);
    Len -= (BlockCount << MEOW_HASH_BLOCK_SIZE_SHIFT);
    while(BlockCount--)
    {
        S0 = Meow128_AESDEC_Mem(S0, Source);
        S1 = Meow128_AESDEC_Mem(S1, Source + 16);
        S2 = Meow128_AESDEC_Mem(S2, Source + 32);
        S3 = Meow128_AESDEC_Mem(S3, Source + 48);
        S4 = Meow128_AESDEC_Mem(S4, Source + 64);
        S5 = Meow128_AESDEC_Mem(S5, Source + 80);
        S6 = Meow128_AESDEC_Mem(S6, Source + 96);
        S7 = Meow128_AESDEC_Mem(S7, Source + 112);
        S8 = Meow128_AESDEC_Mem(S8, Source + 128);
        S9 = Meow128_AESDEC_Mem(S9, Source + 144);
        SA = Meow128_AESDEC_Mem(SA, Source + 160);
        SB = Meow128_AESDEC_Mem(SB, Source + 176);
        SC = Meow128_AESDEC_Mem(SC, Source + 192);
        SD = Meow128_AESDEC_Mem(SD, Source + 208);
        SE = Meow128_AESDEC_Mem(SE, Source + 224);
        SF = Meow128_AESDEC_Mem(SF, Source + 240);
        
        Source += (1 << MEOW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    //
    // NOTE(casey): Handle as many full 128-bit lanes as possible (15 cycles at length 15)
    //
    
    switch(Len >> 4)
    {
        case 15: SE = Meow128_AESDEC_Mem(SE, Source + 224);
        case 14: SD = Meow128_AESDEC_Mem(SD, Source + 208);
        case 13: SC = Meow128_AESDEC_Mem(SC, Source + 192);
        case 12: SB = Meow128_AESDEC_Mem(SB, Source + 176);
        case 11: SA = Meow128_AESDEC_Mem(SA, Source + 160);
        case 10: S9 = Meow128_AESDEC_Mem(S9, Source + 144);
        case  9: S8 = Meow128_AESDEC_Mem(S8, Source + 128);
        case  8: S7 = Meow128_AESDEC_Mem(S7, Source + 112);
        case  7: S6 = Meow128_AESDEC_Mem(S6, Source + 96);
        case  6: S5 = Meow128_AESDEC_Mem(S5, Source + 80);
        case  5: S4 = Meow128_AESDEC_Mem(S4, Source + 64);
        case  4: S3 = Meow128_AESDEC_Mem(S3, Source + 48);
        case  3: S2 = Meow128_AESDEC_Mem(S2, Source + 32);
        case  2: S1 = Meow128_AESDEC_Mem(S1, Source + 16);
        case  1: S0 = Meow128_AESDEC_Mem(S0, Source);
        default:;
    }
    Source += (Len & 0xF0);
    
    //
    // NOTE(casey): Start as much of the mixdown as we can before handling the overhang
    //
    
    // TODO(casey): There needs to be a solid idea behind the mixing vector here.
    // Before Meow v1, we need some definitive analysis of what it should be.
    meow_u128 Mixer = Meow128_Set64x2(Seed - TotalLengthInBytes, Seed + TotalLengthInBytes + 1);
    
    S0 = Meow128_AESDEC(S0, S8);
    S1 = Meow128_AESDEC(S1, S9);
    S2 = Meow128_AESDEC(S2, SA);
    S3 = Meow128_AESDEC(S3, SB);
    S4 = Meow128_AESDEC(S4, SC);
    S5 = Meow128_AESDEC(S5, SD);
    S6 = Meow128_AESDEC(S6, SE);
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    S4 = Meow128_AESDEC(S4, Mixer);
    
    //
    // NOTE(casey): Deal with individual bytes (5 cycles)
    //
    
    // TODO(casey): Probably do a different code path for ARM here!
    // Do they have proper masked gather?
    switch(Len & 0xF)
    {
        case 15: // NOTE(casey): 01234567 89AB CD E
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 14), 14);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 12), 6);
            Partial = _mm_insert_epi32(Partial, *(meow_u32 *)(Source + 8), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case 14: // NOTE(casey): 01234567 89AB CD
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 12), 6);
            Partial = _mm_insert_epi32(Partial, *(meow_u32 *)(Source + 8), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case 13: // NOTE(casey): 01234567 89AB C
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 12), 12);
            Partial = _mm_insert_epi32(Partial, *(meow_u32 *)(Source + 8), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case 12: // NOTE(casey): 01234567 89AB
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi32(Partial, *(meow_u32 *)(Source + 8), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
            
        case 11: // NOTE(casey): 01234567 89 A
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 10), 10);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 8), 4);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case 10: // NOTE(casey): 01234567 89
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 8), 4);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  9: // NOTE(casey): 01234567 8
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 8), 8);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  8: // NOTE(casey): 01234567
        {
            meow_u128 Partial = _mm_loadu_si64(Source);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  7: // NOTE(casey): 0123 45 6
        {
            meow_u128 Partial = _mm_loadu_si32(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 6), 6);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 4), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  6: // NOTE(casey): 0123 45
        {
            meow_u128 Partial = _mm_loadu_si32(Source);
            Partial = _mm_insert_epi16(Partial, *(meow_u16 *)(Source + 4), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  5: // NOTE(casey): 0123 4
        {
            meow_u128 Partial = _mm_loadu_si32(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 4), 4);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  4: // NOTE(casey): 0123
        {
            meow_u128 Partial = _mm_loadu_si32(Source);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  3: // NOTE(casey): 01 2
        {
            meow_u128 Partial = _mm_loadu_si16(Source);
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)(Source + 2), 2);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  2: // NOTE(casey): 01
        {
            meow_u128 Partial = _mm_loadu_si16(Source);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        case  1: // NOTE(casey): 0
        {
            meow_u128 Partial = Meow128_Zero();
            Partial = _mm_insert_epi8(Partial, *(meow_u8 *)Source, 0);
            SF = Meow128_AESDEC(SF, Partial);
        } break;
        
        default:
        {
        } break;
    }
    
    //
    // NOTE(casey): Finish the part of the mixdown that is dependent on SF
    // and then do the tree reduction (starting the tree reduction early
    // doesn't seem to save anything)
    //
        
    S5 = Meow128_AESDEC(S5, Mixer);
    S6 = Meow128_AESDEC(S6, Mixer);
    
    S7 = Meow128_AESDEC(S7, SF);
    S7 = Meow128_AESDEC(S7, Mixer);
    
    S0 = Meow128_AESDEC(S0, S4);
    S1 = Meow128_AESDEC(S1, S5);
    S2 = Meow128_AESDEC(S2, S6);
    S3 = Meow128_AESDEC(S3, S7);
    
    S0 = Meow128_AESDEC(S0, S2);
    S1 = Meow128_AESDEC(S1, S3);
    
    S0 = Meow128_AESDEC(S0, S1);
    S0 = Meow128_AESDEC(S0, Mixer);
    
    return(S0);
}

#if MEOW_HASH_AVX512

//
// NOTE(casey): 256-wide VAES Meow (maximum of 32 bytes/clock single threaded)
//

static meow_macroblock_result
MeowHash2Op(int BlockCount, meow_u8 *Source)
{
    meow_u256 S01 = Meow256_Zero();
    meow_u256 S23 = Meow256_Zero();
    meow_u256 S45 = Meow256_Zero();
    meow_u256 S67 = Meow256_Zero();
    meow_u256 S89 = Meow256_Zero();
    meow_u256 SAB = Meow256_Zero();
    meow_u256 SCD = Meow256_Zero();
    meow_u256 SEF = Meow256_Zero();
    
    while(BlockCount--)
    {
        S01 = Meow256_AESDEC_Mem(S01, Source);
        S23 = Meow256_AESDEC_Mem(S23, Source + 32);
        
        S45 = Meow256_AESDEC_Mem(S45, Source + 64);
        S67 = Meow256_AESDEC_Mem(S67, Source + 96);
        
        S89 = Meow256_AESDEC_Mem(S89, Source + 128);
        SAB = Meow256_AESDEC_Mem(SAB, Source + 160);
        
        SCD = Meow256_AESDEC_Mem(SCD, Source + 192);
        SEF = Meow256_AESDEC_Mem(SEF, Source + 224);
        
        Source += (1 << MEOW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    meow_macroblock_result Result;
    _mm256_store_si256(&Result.S0, S01);
    _mm256_store_si256(&Result.S2, S23);
    _mm256_store_si256(&Result.S4, S45);
    _mm256_store_si256(&Result.S6, S67);
    _mm256_store_si256(&Result.S8, S89);
    _mm256_store_si256(&Result.SA, SAB);
    _mm256_store_si256(&Result.SC, SCD);
    _mm256_store_si256(&Result.SE, SEF);
    
    return(Result);
}

static meow_u128
MeowHash2(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *Source)
{
    // TODO(casey): Once we can test, do the optimized non-megabyte version for 256-bit
    meow_u128 Result = MeowHashViaOp(MeowHash2Op, Seed, TotalLengthInBytes, Source);
    return(Result);
}

//
// NOTE(casey): 512-wide VAES Meow (maximum of 64 bytes/clock single threaded)
//

static meow_macroblock_result
MeowHash4Op(int BlockCount, meow_u8 *Source)
{
    meow_u512 S0123 = Meow512_Zero();
    meow_u512 S4567 = Meow512_Zero();
    meow_u512 S89AB = Meow512_Zero();
    meow_u512 SCDEF = Meow512_Zero();
    
    while(BlockCount--)
    {
        S0123 = Meow512_AESDEC_Mem(S0123, From);
        S4567 = Meow512_AESDEC_Mem(S4567, From + 64);
        S89AB = Meow512_AESDEC_Mem(S89AB, From + 128);
        SCDEF = Meow512_AESDEC_Mem(SCDEF, From + 192);
        
        Source += (1 << MEOW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    meow_macroblock_result Result;
    _mm512_store_si512(&Result.S0, S0123);
    _mm512_store_si512(&Result.S4, S4567);
    _mm512_store_si512(&Result.S8, S89AB);
    _mm512_store_si512(&Result.SC, SCDEF);
    
    return(Result);
}

static meow_u128
MeowHash4(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *Source)
{
    // TODO(casey): Once we can test, do the optimized non-megabyte version for 512-bit
    meow_u128 Result = MeowHashViaOp(MeowHash4Op, Seed, TotalLengthInBytes, Source);
    return(Result);
}

#endif
