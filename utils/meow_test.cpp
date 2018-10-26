/* ========================================================================

   meow_test.cpp - basic sanity checking for any build of Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "meow_test.h"

//
// NOTE(casey): Minimalist code for Meow testing.
//
// This is NOT a replacement for the real hash testing (done via smhasher, etc.)
// It is just a brief sanity check to ensure that your Meow compilation is
// working correctly.
//

int
main(int ArgCount, char **Args)
{
    int Result = 0;
    
    // NOTE(casey): Print the banner
    printf("meow_test %s - basic sanity test for a Meow hash build\n", MEOW_HASH_VERSION_NAME);
    printf("    See https://mollyrocket.com/meowhash for details\n");
    printf("\n");
    
    printf("Unaligned sources: ");
    {
        meow_u8 *Test = (meow_u8 *)aligned_alloc(MEOW_HASH_ALIGNMENT, 257);
        TRY
        {
            MeowHash1(0, 256, Test + 1);
            printf("supported");
        }
        CATCH
        {
            printf("UNSUPPORTED");
        }
        free(Test);
    }
    printf("\n");
    
    for(int TypeIndex = 0;
        TypeIndex < ArrayCount(NamedHashTypes);
        ++TypeIndex)
    {
        named_hash_type *Type = NamedHashTypes + TypeIndex;
        if(Type->Op)
        {
            int TotalPossible = 0;
            int ImpError = 0;
            int OpError = 0;
            int Unsupported = 0;
            
            printf("%s: ", Type->FullName);
            
            for(int BufferSize = 1;
                BufferSize <= 2048;
                ++BufferSize)
            {
                int AllocationSize = BufferSize + 2*MEOW_HASH_ALIGNMENT;
                meow_u8 *Allocation = (meow_u8 *)aligned_alloc(MEOW_HASH_ALIGNMENT, AllocationSize);
                memset(Allocation, 0, AllocationSize);
                
                meow_u8 *Buffer = Allocation + MEOW_HASH_ALIGNMENT;
                for(int Guard = 0;
                    Guard < 1;
                    ++Guard)
                {
                    for(int Flip = 0;
                        Flip < BufferSize;
                        ++Flip)
                    {
                        meow_u64 Seed = 0;
                        
                        meow_u8 *FlipByte = Buffer + (Flip / 8);
                        meow_u8 FlipBit = (1 << (Flip % 8));
                        *FlipByte |= FlipBit;
                        
                        meow_hash Canonical = MeowHash1(Seed, BufferSize, Buffer);
                        if(Guard)
                        {
                            memset(Allocation, 0xFF, MEOW_HASH_ALIGNMENT);
                            memset(Allocation + MEOW_HASH_ALIGNMENT + BufferSize, 0xFF, MEOW_HASH_ALIGNMENT);
                        }
                        
                        ++TotalPossible;
                        TRY
                        {
                            meow_hash ImpHash = Type->Imp(Seed, BufferSize, Buffer);
                            meow_hash OpHash = MeowHashViaOp(Type->Op, Seed, BufferSize, Buffer);
                            
                            if(!MeowHashesAreEqual(Canonical, ImpHash))
                            {
                                ++ImpError;
                            }
                            
                            if(!MeowHashesAreEqual(Canonical, OpHash))
                            {
                                ++OpError;
                            }
                        }
                        CATCH
                        {
                            ++Unsupported;
                            break;
                        }
                        
                        if(Guard)
                        {
                            memset(Allocation, 0, MEOW_HASH_ALIGNMENT);
                            memset(Allocation + MEOW_HASH_ALIGNMENT + BufferSize, 0, MEOW_HASH_ALIGNMENT);
                        }
                        
                        *FlipByte &= ~FlipBit;
                    }
                }
                
                free(Allocation);
            }
            
            if(Unsupported)
            {
                printf("UNSUPPORTED");
            }
            else
            {
                if(ImpError || OpError)
                {
                    printf("FAILED");
                    if(ImpError)
                    {
                        printf(" [direct:%u/%u]", ImpError, TotalPossible);
                    }
                    
                    if(OpError)
                    {
                        printf(" [op:%u/%u]", OpError, TotalPossible);
                    }
                    
                    Result = -1;
                }
                else
                {
                    printf("PASSED");
                }
            }
            printf("\n");
        }
    }
    
    return(Result);
}
