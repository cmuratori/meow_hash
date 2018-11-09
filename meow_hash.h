/* ========================================================================

   Meow - A Fast Non-cryptographic Hash
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
      on x64 and ARM processors that provide AES instructions.  It is
      designed to be truncatable to 64 and 32-bit hash values and still
      retain good collision resistance.
      
   Q: What is it GOOD for?
   
   A: Quickly hashing any amount of data for comparison purposes such as
      block deduplication or change detection.  It is extremely fast on
      all buffer sizes, from one byte to one gigabyte and up.
      
   Q: What is it BAD for?
   
   A: Anything security-related.  It should be assumed that it provides
      no protection from adversaries whatsoever.  It is also not particularly
      fast on processors that don't support AES instructions (eg., non-x64/ARM
      processors).
      
   Q: Why is it called the "Meow hash"?
   
   A: It is named after a character in Meow the Infinite
      (https://meowtheinfinite.com)
      
   Q: Who wrote it?
   
   A: CASEY MURATORI (https://caseymuratori.com) wrote the original
      implementation for use in processing large-footprint assets for
      the game 1935 (https://molly1935.com).
      
      After the initial version, the hash was refined via collaboration
      with several great programmers who contributed suggestions and
      modifications:
      
      JEFF ROBERTS (https://radgametools.com) provided a super slick
      way to handle the residual end-of-buffer bytes that dramatically
      improved Meow's small hash performance.
      
      MARTINS MOZEIKO (https://matrins.ninja) ported Meow to ARM
      and added the proper preprocessor dressing for clean compilation
      on a variety of compiler configurations.
      
      FABIAN GIESEN (https://fgiesen.wordpress.com) provided support
      for getting the benchmarking working properly across a number
      of platforms.
      
      ARAS PRANCKEVICIUS (https://aras-p.info) provided the allocation
      shim for compilation on Mac OS X.
      
   ========================================================================
   
   USAGE
   
   For a complete working example, see meow_example.cpp.  Briefly:
   
       // Include meow_intrinsics if you want it to detect platforms
       // and define types and intrinsics for you.  Omit it if you
       // want to define them yourself.
       #include "meow_intrinsics.h"
       
       // Include meow_hash for the Meow hash function
       #include "meow_hash.h"
       
       // Hash a block of data using CPU-specific acceleration
       meow_u128 MeowHash_Accelerated(u64 Seed, u64 Len, void *Source);
       
       // Check if two Meow hashes are the same
       // (returns zero if they aren't, non-zero if they are)
       int MeowHashesAreEqual(meow_u128 A, meow_u128 B)
       
       // Truncate a Meow hash to 64 bits
       meow_u64 MeowU64From(meow_u128 Hash);
       
       // Truncate a Meow hash to 32 bits
       meow_u32 MeowU32From(meow_u128 Hash);
       
   **** VERY IMPORTANT X64 COMPILATION NOTES ****
   
   On x64, Meow uses the AESDEC instruction, which comes in two flavors:
   SSE (aesdec) and AVX (vaesdec).  If you are compiling _with_ AVX support,
   your compiler will probably emit the AVX variant, which means your code
   WILL NOT RUN on computers that do not have AVX.  If you need to deploy
   this hash on computers that do not have AVX, you must take care to
   TURN OFF support for AVX in your compiler for the file that includes
   the Meow hash!
   
   ======================================================================== */

//
// NOTE(casey): This version is EXPERIMENTAL.  The Meow hash is still
// undergoing testing and finalization.
//
// **** EXPECT HASHES/APIs TO CHANGE UNTIL THE VERSION NUMBER HITS 1.0. ****
//
// You have been warned.
//

static const unsigned char MeowShiftAdjust[31] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128};
static const unsigned char MeowMaskLen[32] = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

// TODO(casey): These constants are loaded to initialize the lanes.  Jacob should
// give us some feedback on what they should _actually_ be set to.
#define MEOW_S0_INIT {0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15}
#define MEOW_S1_INIT {16,17,18,19, 20,21,22,23, 24,25,26,27, 28,29,30,31}
#define MEOW_S2_INIT {32,33,34,35, 36,37,38,39, 40,41,42,43, 44,45,46,47}
#define MEOW_S3_INIT {48,49,50,51, 52,53,54,55, 56,57,58,59, 60,61,62,63}
static const unsigned char MeowS0Init[] = MEOW_S0_INIT;
static const unsigned char MeowS1Init[] = MEOW_S1_INIT;
static const unsigned char MeowS2Init[] = MEOW_S2_INIT;
static const unsigned char MeowS3Init[] = MEOW_S3_INIT;

//
// NOTE(casey): 128-wide AES-NI Meow (maximum of 16 bytes/clock single threaded)
//

static meow_u128
MeowHash_Accelerated(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    //
    // NOTE(casey): Initialize all 16 streams to 0
    //
    
    // TODO(casey): Hopefully the compiler is smart enough to generate the correct aligned/unaligned
    // load instruction here, since it knows the address at compile time?
    meow_aes_128 S0 = *(meow_u128 *)MeowS0Init;
    meow_aes_128 S2 = *(meow_u128 *)MeowS2Init;
    meow_aes_128 S3 = *(meow_u128 *)MeowS3Init;
    meow_aes_128 S1 = *(meow_u128 *)MeowS1Init;
    
    //
    // NOTE(casey): Handle as many full 256-byte blocks as possible
    //
    
    meow_u8 *Source = (meow_u8 *)SourceInit;
    meow_u64 Len = TotalLengthInBytes;
    meow_u64 BlockCount = (Len >> 6);
    Len -= (BlockCount << 6);
    while(BlockCount--)
    {
        S0 = Meow128_AESDEC_Mem(S0, Source);
        S1 = Meow128_AESDEC_Mem(S1, Source + 16);
        S2 = Meow128_AESDEC_Mem(S2, Source + 32);
        S3 = Meow128_AESDEC_Mem(S3, Source + 48);
        
        Source += 64;
    }
    
    //
    // NOTE(casey): Handle as many full 128-bit lanes as possible
    //
    
    switch(Len >> 4)
    {
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

    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    
    //
    // NOTE(casey): Deal with individual bytes
    //

    if(Len & 15)
    {
        Len &= 15;
        int Align = ((int)(meow_umm)Source) & 15;
        if(Align)
        {
            int End = ((int)(meow_umm)Source) & (MEOW_PAGESIZE - 1);
        
            // NOTE(jeffr): If we are nowhere near the page end, use full unaligned load (cmov to set)
            if (End <= (MEOW_PAGESIZE - 16))
            {
                Align = 0;
            }
            
            // NOTE(jeffr): If we will read over the page end, use a full unaligned load (cmov to set)
            if ((End + Len) > MEOW_PAGESIZE)
            {
                Align = 0;
            }
            
            meow_aes_128 PartialState = Meow128_Shuffle_Mem(Source - Align, &MeowShiftAdjust[Align]);
            
            PartialState = Meow128_And_Mem( PartialState, &MeowMaskLen[16 - Len] );
            S3 = Meow128_AESDEC(S3, PartialState);
        }
        else
        {
            meow_u128 PartialState = Meow128_And_Mem(*(meow_u128 *)Source, &MeowMaskLen[16 - Len]);
            S3 = Meow128_AESDEC(S3, PartialState);
        }
    }
    
    //
    // NOTE(casey): Finish the part of the mixdown that is dependent on S3
    // and then do the tree reduction (starting the tree reduction early
    // doesn't seem to save anything)
    //
    
    S3 = Meow128_AESDEC(S3, Mixer);
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S2));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S3));
    S1 = Meow128_AESDEC(S1, Mixer);
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S1));
    S0 = Meow128_AESDEC(S0, Mixer);
    
    meow_u128 Result = Meow128_AESDEC_Finalize(S0);
    return(Result);
}

#if !defined(MEOW_HELPERS)

static meow_u64
MeowU64From(meow_u128 Hash)
{
    // TODO(casey): It is probably worth it to use the cvt intrinsics here
    // TODO(mmozeiko): use vgetq_lane_u64 on ARMv8
    meow_u64 Result = *(meow_u64 *)&Hash;
    return(Result);
}

static meow_u32
MeowU32From(meow_u128 Hash)
{
    // TODO(casey): It is probably worth it to use the cvt intrinsics here
    // TODO(mmozeiko): use vgetq_lane_u32 on ARMv8
    meow_u32 Result = *(meow_u32 *)&Hash;
    return(Result);
}

static int
MeowHashesAreEqual(meow_u128 A, meow_u128 B)
{
    int Result = Meow128_AreEqual(A, B);
    return(Result);
}

#define MEOW_HELPERS
#endif
