/* ========================================================================

   Megapaw - Speculative hash function for future VAES-enabled CPUs
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */
   
//
// NOTE(casey): 128-wide AES-NI Megapaw (maximum of 16 bytes/clock single threaded)
//

#define MEGAPAW_HASH_BLOCK_SIZE_SHIFT 8

static const unsigned char MegapawShiftAdjust[31] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128};
static const unsigned char MegapawMaskLen[32] = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

//
// NOTE(casey): 128-wide AES-NI Meow (maximum of 16 bytes/clock single threaded)
//

static meow_u128
MegapawHash_128Wide(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    //
    // NOTE(casey): Initialize all 16 streams to 0
    //
    
    meow_aes_128 S0 = Meow128_ZeroState();
    meow_aes_128 S1 = Meow128_ZeroState();
    meow_aes_128 S2 = Meow128_ZeroState();
    meow_aes_128 S3 = Meow128_ZeroState();
    meow_aes_128 S4 = Meow128_ZeroState();
    meow_aes_128 S5 = Meow128_ZeroState();
    meow_aes_128 S6 = Meow128_ZeroState();
    meow_aes_128 S7 = Meow128_ZeroState();
    meow_aes_128 S8 = Meow128_ZeroState();
    meow_aes_128 S9 = Meow128_ZeroState();
    meow_aes_128 SA = Meow128_ZeroState();
    meow_aes_128 SB = Meow128_ZeroState();
    meow_aes_128 SC = Meow128_ZeroState();
    meow_aes_128 SD = Meow128_ZeroState();
    meow_aes_128 SE = Meow128_ZeroState();
    meow_aes_128 SF = Meow128_ZeroState();
    
    //
    // NOTE(casey): Handle as many full 256-byte blocks as possible (16 cycles per block)
    //
    
    meow_u8 *Source = (meow_u8 *)SourceInit;
    meow_u64 Len = TotalLengthInBytes;
    meow_u64 BlockCount = (Len >> MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    Len -= (BlockCount << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
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
        
        Source += (1 << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
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
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S8));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S9));
    S2 = Meow128_AESDEC(S2, Meow128_AESDEC_Finalize(SA));
    S3 = Meow128_AESDEC(S3, Meow128_AESDEC_Finalize(SB));
    S4 = Meow128_AESDEC(S4, Meow128_AESDEC_Finalize(SC));
    S5 = Meow128_AESDEC(S5, Meow128_AESDEC_Finalize(SD));
    S6 = Meow128_AESDEC(S6, Meow128_AESDEC_Finalize(SE));
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    S4 = Meow128_AESDEC(S4, Mixer);
    S5 = Meow128_AESDEC(S5, Mixer);
    S6 = Meow128_AESDEC(S6, Mixer);
    
    //
    // NOTE(casey): Deal with individual bytes
    //
    
    if(Len & 15)
    {
        int Align = ((int)(meow_umm)Source) & 15;
        int End = ((int)(meow_umm)Source) & (MEOW_PAGESIZE - 1);
        Len &= 15;
        
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
        
        meow_aes_128 PartialState = Meow128_Shuffle_Mem(Source - Align, &MegapawShiftAdjust[Align]);
        
        PartialState = Meow128_And_Mem( PartialState, &MegapawMaskLen[16 - Len] );
        SF = Meow128_AESDEC(PartialState, Meow128_AESDEC_Finalize(SF));
    }
    
    //
    // NOTE(casey): Finish the part of the mixdown that is dependent on SF
    // and then do the tree reduction (starting the tree reduction early
    // doesn't seem to save anything)
    //
    
    S7 = Meow128_AESDEC(S7, Meow128_AESDEC_Finalize(SF));
    S7 = Meow128_AESDEC(S7, Mixer);
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S4));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S5));
    S2 = Meow128_AESDEC(S2, Meow128_AESDEC_Finalize(S6));
    S3 = Meow128_AESDEC(S3, Meow128_AESDEC_Finalize(S7));
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S2));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S3));
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S1));
    S0 = Meow128_AESDEC(S0, Mixer);
    
    meow_u128 Result = Meow128_AESDEC_Finalize(S0);
    return(Result);
}

//
// NOTE(casey): 256-wide VAES Meow (maximum of 32 bytes/clock single threaded)
//

static meow_u128
MegapawHash_256Wide(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    meow_u256 S01 = Meow256_Zero();
    meow_u256 S23 = Meow256_Zero();
    meow_u256 S45 = Meow256_Zero();
    meow_u256 S67 = Meow256_Zero();
    meow_u256 S89 = Meow256_Zero();
    meow_u256 SAB = Meow256_Zero();
    meow_u256 SCD = Meow256_Zero();
    meow_u256 SEF = Meow256_Zero();
    
    //
    // NOTE(casey): Handle as many full 256-byte blocks as possible (4 cycles per block)
    //
    
    meow_u8 *Source = (meow_u8 *)SourceInit;
    meow_u64 Len = TotalLengthInBytes;
    meow_u64 BlockCount = (Len >> MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    Len -= (BlockCount << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
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
        
        Source += (1 << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    //
    // NOTE(casey): Handle as many full 32-byte blocks as possible
    //
    
    switch(Len >> 5)
    {
        case 7:
        SCD = Meow256_AESDEC_Mem(SCD, Source + 192);
        case 6:
        SAB = Meow256_AESDEC_Mem(SAB, Source + 160);
        case 5:
        S89 = Meow256_AESDEC_Mem(S89, Source + 128);
        case 4:
        S67 = Meow256_AESDEC_Mem(S67, Source + 96);
        case 3:
        S45 = Meow256_AESDEC_Mem(S45, Source + 64);
        case 2:
        S23 = Meow256_AESDEC_Mem(S23, Source + 32);
        case 1:
        S01 = Meow256_AESDEC_Mem(S01, Source);
        default:;
    }
    
    if(Len & 0x1f)
    {
        meow_u256 Partial = Meow256_PartialLoad(Source + (Len & 0xE0), Len & 0x1F);
        SEF = Meow256_AESDEC(Partial, SEF);
    }
    
    S01 = Meow256_AESDEC(S01, S89);
    S23 = Meow256_AESDEC(S23, SAB);
    S45 = Meow256_AESDEC(S45, SCD);
    S67 = Meow256_AESDEC(S67, SEF);
    
    S01 = Meow256_AESDEC(S01, Mixer4);
    S23 = Meow256_AESDEC(S23, Mixer4);
    S45 = Meow256_AESDEC(S45, Mixer4);
    S67 = Meow256_AESDEC(S67, Mixer4);
    
    S01 = Meow256_AESDEC(S01, S45);
    S23 = Meow256_AESDEC(S23, S67);
    
    S01 = Meow256_AESDEC(S01, S23);
    
    meow_u128 S0 = Meow128FromLow(S01);
    meow_u128 S1 = Meow128FromHIgh(S01);
    
    S0 = Meow128_AESDEC(S0, S1);
    S0 = Meow128_AESDEC(S0, Mixer1);
    
    return(S0);
    return(Result);
}

//
// NOTE(casey): 512-wide VAES Meow (maximum of 64 bytes/clock single threaded)
//

static meow_u128
MegapawHash_512Wide(meow_u64 Seed, meow_u64 TotalLengthInBytes, void *SourceInit)
{
    //
    // NOTE(casey): Initialize all 16 streams to 0
    //
    
    meow_u512 S0123 = Meow512_Zero();
    meow_u512 S4567 = Meow512_Zero();
    meow_u512 S89AB = Meow512_Zero();
    meow_u512 SCDEF = Meow512_Zero();
    
    //
    // NOTE(casey): Handle as many full 256-byte blocks as possible (4 cycles per block)
    //
    
    meow_u8 *Source = (meow_u8 *)SourceInit;
    meow_u64 Len = TotalLengthInBytes;
    meow_u64 BlockCount = (Len >> MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    Len -= (BlockCount << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    while(BlockCount--)
    {
        S0123 = Meow512_AESDEC_Mem(S0123, Source);
        S4567 = Meow512_AESDEC_Mem(S4567, Source + 64);
        S89AB = Meow512_AESDEC_Mem(S89AB, Source + 128);
        SCDEF = Meow512_AESDEC_Mem(SCDEF, Source + 192);
        
        Source += (1 << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    //
    // NOTE(casey): Handle as many full 64-byte blocks as possible
    //
    
    switch(Len >> 6)
    {
        case 3:
        S89AB = Meow512_AESDEC_Mem(S89AB, Source + 128);
        case 2:
        S4567 = Meow512_AESDEC_Mem(S4567, Source + 64);
        case 1:
        S0123 = Meow512_AESDEC_Mem(S0123, Source);
        default:
    }
    
    if(Len & 0x3f)
    {
        meow_u512 Partial = Meow512_PartialLoad(Source + (Len & 0xC0), Len & 0x3F);
        SCDEF = Meow512_AESDEC(Partial, SCDEF);
    }
    
    S0123 = Meow512_AESDEC(S0123, S89AB);
    S4567 = Meow512_AESDEC(S4567, SCDEF);
    
    S0123 = Meow512_AESDEC(S0123, Mixer4);
    S4567 = Meow512_AESDEC(S4567, Mixer4);
    
    S0123 = Meow512_AESDEC(S0123, S4567);
    
    meow_u256 S01 = Meow256FromLow(S0123);
    meow_u256 S23 = Meow256FromHigh(S0123);
    S01 = Meow256_AESDEC(S01, S23);
    
    meow_u128 S0 = Meow128FromLow(S01);
    meow_u128 S1 = Meow128FromHIgh(S01);
    
    S0 = Meow128_AESDEC(S0, S1);
    S0 = Meow128_AESDEC(S0, Mixer1);
    
    return(S0);
}

//
// NOTE(casey): Streaming construction (optional)
//

typedef struct meow_hash_state
{
    union
    {
        struct
        {
            meow_aes_128 S0;
            meow_aes_128 S1;
            meow_aes_128 S2;
            meow_aes_128 S3;
            meow_aes_128 S4;
            meow_aes_128 S5;
            meow_aes_128 S6;
            meow_aes_128 S7;
            meow_aes_128 S8;
            meow_aes_128 S9;
            meow_aes_128 SA;
            meow_aes_128 SB;
            meow_aes_128 SC;
            meow_aes_128 SD;
            meow_aes_128 SE;
            meow_aes_128 SF;
        };
        
        struct
        {
            meow_aes_256 S01;
            meow_aes_256 S23;
            meow_aes_256 S45;
            meow_aes_256 S67;
            meow_aes_256 S89;
            meow_aes_256 SAB;
            meow_aes_256 SCD;
            meow_aes_256 SEF;
        };
        
        struct
        {
            meow_aes_512 S0123;
            meow_aes_512 S4567;
            meow_aes_512 S89AB;
            meow_aes_512 SCDEF;
        };
    };
    
    meow_u64 TotalLengthInBytes;
    
    meow_u8 Buffer[1 << MEGAPAW_HASH_BLOCK_SIZE_SHIFT];
    int unsigned BufferLen;
} meow_hash_state;

static void
MegapawHashBegin(meow_hash_state *State)
{
    //
    // NOTE(casey): Initialize all 16 streams to 0
    //
    
    State->S0 = Meow128_ZeroState();
    State->S1 = Meow128_ZeroState();
    State->S2 = Meow128_ZeroState();
    State->S3 = Meow128_ZeroState();
    State->S4 = Meow128_ZeroState();
    State->S5 = Meow128_ZeroState();
    State->S6 = Meow128_ZeroState();
    State->S7 = Meow128_ZeroState();
    State->S8 = Meow128_ZeroState();
    State->S9 = Meow128_ZeroState();
    State->SA = Meow128_ZeroState();
    State->SB = Meow128_ZeroState();
    State->SC = Meow128_ZeroState();
    State->SD = Meow128_ZeroState();
    State->SE = Meow128_ZeroState();
    State->SF = Meow128_ZeroState();
    
    State->TotalLengthInBytes = 0;
    State->BufferLen = 0;
}

static void
MegapawHashAbsorbBlocks1(meow_hash_state *State, meow_u64 BlockCount, meow_u8 *Source)
{
    meow_aes_128 S0 = State->S0;
    meow_aes_128 S1 = State->S1;
    meow_aes_128 S2 = State->S2;
    meow_aes_128 S3 = State->S3;
    meow_aes_128 S4 = State->S4;
    meow_aes_128 S5 = State->S5;
    meow_aes_128 S6 = State->S6;
    meow_aes_128 S7 = State->S7;
    meow_aes_128 S8 = State->S8;
    meow_aes_128 S9 = State->S9;
    meow_aes_128 SA = State->SA;
    meow_aes_128 SB = State->SB;
    meow_aes_128 SC = State->SC;
    meow_aes_128 SD = State->SD;
    meow_aes_128 SE = State->SE;
    meow_aes_128 SF = State->SF;
    
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
        
        Source += (1 << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    }
    
    State->S0 = S0;
    State->S1 = S1;
    State->S2 = S2;
    State->S3 = S3;
    State->S4 = S4;
    State->S5 = S5;
    State->S6 = S6;
    State->S7 = S7;
    State->S8 = S8;
    State->S9 = S9;
    State->SA = SA;
    State->SB = SB;
    State->SC = SC;
    State->SD = SD;
    State->SE = SE;
    State->SF = SF;
}

static void
MegapawHashAbsorb1(meow_hash_state *State, meow_u64 Len, void *SourceInit)
{
    State->TotalLengthInBytes += Len;
    meow_u8 *Source = (meow_u8 *)SourceInit;
    
    // NOTE(casey): Handle any buffered residual
    if(State->BufferLen)
    {
        int unsigned Fill = (sizeof(State->Buffer) - State->BufferLen);
        if(Fill > Len)
        {
            Fill = (int unsigned)Len;
        }
        
        Len -= Fill;
        while(Fill--)
        {
            State->Buffer[State->BufferLen++] = *Source++;
        }
        
        if(State->BufferLen == sizeof(State->Buffer))
        {
            MegapawHashAbsorbBlocks1(State, 1, State->Buffer);
            State->BufferLen = 0;
        }
    }
    
    // NOTE(casey): Handle any full blocks
    meow_u64 BlockCount = (Len >> MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    meow_u64 Advance = (BlockCount << MEGAPAW_HASH_BLOCK_SIZE_SHIFT);
    MegapawHashAbsorbBlocks1(State, BlockCount, Source);
    
    Len -= Advance;
    Source += Advance;
    
    // NOTE(casey): Store residual
    while(Len--)
    {
        State->Buffer[State->BufferLen++] = *Source++;
    }
}

static meow_u128
MegapawHashEnd(meow_hash_state *State, meow_u64 Seed)
{
    meow_aes_128 S0 = State->S0;
    meow_aes_128 S1 = State->S1;
    meow_aes_128 S2 = State->S2;
    meow_aes_128 S3 = State->S3;
    meow_aes_128 S4 = State->S4;
    meow_aes_128 S5 = State->S5;
    meow_aes_128 S6 = State->S6;
    meow_aes_128 S7 = State->S7;
    meow_aes_128 S8 = State->S8;
    meow_aes_128 S9 = State->S9;
    meow_aes_128 SA = State->SA;
    meow_aes_128 SB = State->SB;
    meow_aes_128 SC = State->SC;
    meow_aes_128 SD = State->SD;
    meow_aes_128 SE = State->SE;
    meow_aes_128 SF = State->SF;
    
    meow_u8 *Source = State->Buffer;
    int unsigned Len = State->BufferLen;
    
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
    // NOTE(casey): Deal with individual bytes
    //
    
    if(Len & 15)
    {
        int Align = ((int)(meow_umm)Source) & 15;
        int End = ((int)(meow_umm)Source) & (MEOW_PAGESIZE - 1);
        Len &= 15;
        
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
        
        meow_aes_128 PartialState = Meow128_Shuffle_Mem(Source - Align, &MegapawShiftAdjust[Align]);
        
        PartialState = Meow128_And_Mem( PartialState, &MegapawMaskLen[16 - Len] );
        SF = Meow128_AESDEC(PartialState, Meow128_AESDEC_Finalize(SF));
    }
    
    meow_u128 Mixer = Meow128_Set64x2(Seed - State->TotalLengthInBytes,
                                      Seed + State->TotalLengthInBytes + 1);
                                      
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S8));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S9));
    S2 = Meow128_AESDEC(S2, Meow128_AESDEC_Finalize(SA));
    S3 = Meow128_AESDEC(S3, Meow128_AESDEC_Finalize(SB));
    S4 = Meow128_AESDEC(S4, Meow128_AESDEC_Finalize(SC));
    S5 = Meow128_AESDEC(S5, Meow128_AESDEC_Finalize(SD));
    S6 = Meow128_AESDEC(S6, Meow128_AESDEC_Finalize(SE));
    S7 = Meow128_AESDEC(S7, Meow128_AESDEC_Finalize(SF));
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    S4 = Meow128_AESDEC(S4, Mixer);
    S5 = Meow128_AESDEC(S5, Mixer);
    S6 = Meow128_AESDEC(S6, Mixer);
    S7 = Meow128_AESDEC(S7, Mixer);
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S4));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S5));
    S2 = Meow128_AESDEC(S2, Meow128_AESDEC_Finalize(S6));
    S3 = Meow128_AESDEC(S3, Meow128_AESDEC_Finalize(S7));
    
    S0 = Meow128_AESDEC(S0, Mixer);
    S1 = Meow128_AESDEC(S1, Mixer);
    S2 = Meow128_AESDEC(S2, Mixer);
    S3 = Meow128_AESDEC(S3, Mixer);
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S2));
    S1 = Meow128_AESDEC(S1, Meow128_AESDEC_Finalize(S3));
    
    S0 = Meow128_AESDEC(S0, Meow128_AESDEC_Finalize(S1));
    S0 = Meow128_AESDEC(S0, Mixer);
    
    meow_u128 Result = Meow128_AESDEC_Finalize(S0);
    return(Result);
}

#if !defined(MEOW_HELPERS)
//
// NOTE(casey): Smaller hash extraction
//

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

//
// NOTE(casey): "Fast" comparison (using SSE or NEON)
//

static int
MeowHashesAreEqual(meow_u128 A, meow_u128 B)
{
    int Result = Meow128_AreEqual(A, B);
    return(Result);
}
#define MEOW_HELPERS
#endif
