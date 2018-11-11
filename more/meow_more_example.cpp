/* ========================================================================

   meow_more_example.cpp - more basic usage examples of the Meow hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

//
// NOTE(casey): Step 1 - include an intrinsics header, then include meow_hash.h
//
// Meow relies on definitions for non-standard types (meow_u128, etc.) and
// intrinsics for various platforms. You can either include the supplied meow_intrinsics.h
// file that will define these for you with its best guesses for your platform, or for
// more control, you can define them all yourself to map to your own stuff.
//

#define MEOW_INCLUDE_C 1 // NOTE(casey): Require the C version, since we'll use it
#include "meow_intrinsics.h" // NOTE(casey): Platform prerequisites for the Meow hash code (replace with your own, if you want)
#include "meow_hash.h" // NOTE(casey): The Meow hash code itself

//
// NOTE(casey): Step 2 - include the extra Meow hash header
//

#include "meow_more.h"

//
// NOTE(casey): Step 3 - detect which Meow hash the CPU can run
//

static meow_hash_implementation *MeowHash = MeowHash_C;
int MeowHashSpecializeForCPU(void)
{
    int Result = 0;
    
    try
    {
        char Garbage[64];
        MeowHash_Accelerated(0, sizeof(Garbage), Garbage);
        MeowHash = MeowHash_Accelerated;
        Result = 128;
    }
    catch(...)
    {
        MeowHash = MeowHash_C;
        Result = 64;
    }
    
    return(Result);
}


//
// NOTE(casey): That's it!  Everything else below here is just boilerplate for starting up
// and loading files with the C runtime library.
//

int
main(int ArgCount, char **Args)
{
    // NOTE(casey): Print the banner
    printf("meow_example %s - basic usage example of the Meow hash\n", MEOW_HASH_VERSION_NAME);
    printf("(C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)\n");
    printf("See https://mollyrocket.com/meowhash for details.\n");
    printf("\n");
    
    // NOTE(casey): Detect which MeowHash to call - do this only once, at startup.
    int BitWidth = MeowHashSpecializeForCPU();
    printf("Using %u-bit Meow implementation\n", BitWidth);
    
    return(0);
}
