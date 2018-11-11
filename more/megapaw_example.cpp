/* ========================================================================

   megapaw_example.cpp - basic usage example of the Megapaw hash
   (C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

//
// NOTE(casey): Step 1 - include an intrinsics header, then include megapaw_hash.h
//
// Megapaw relies on definitions for non-standard types (meow_u128, etc.) and
// intrinsics for various platforms. You can either include the supplied meow_intrinsics.h
// file that will define these for you with its best guesses for your platform, or for
// more control, you can define them all yourself to map to your own stuff.
//

#include "meow_intrinsics.h" // NOTE(casey): Platform prerequisites for the Megapaw hash code (replace with your own, if you want)
#include "megapaw_hash.h" // NOTE(casey): The Megapaw hash code itself

//
// NOTE(casey): Step 2 - detect which Megapaw hash the CPU can run
//

static meow_hash_implementation *MegapawHash = MegapawHash_128Wide;

int MegapawHashSpecializeForCPU(void)
{
    int Result = 0;
    
    try
    {
        char Garbage[64];
        MegapawHash_512Wide(0, sizeof(Garbage), Garbage);
        MegapawHash = MegapawHash_512Wide;
        Result = 512;
    }
    catch(...)
    {
        try
        {
            char Garbage[64];
            MegapawHash_256Wide(0, sizeof(Garbage), Garbage);
            MegapawHash = MegapawHash_256Wide;
            Result = 256;
        }
        catch(...)
        {
            char Garbage[64];
            MegapawHash_128Wide(0, sizeof(Garbage), Garbage);
            MegapawHash = MegapawHash_128Wide;
            Result = 128;
        }
    }
    
    return(Result);
}

//
// NOTE(casey): Step 3 - use the Megapaw hash in a variety of ways!
//
// Example functions below:
//   PrintHash - how to print a Megapaw hash to stdout, from highest-order 32-bits to lowest
//   HashTestBuffer - how to have Megapaw hash a buffer of data
//   HashOneFile - have Megapaw hash the contents of a file
//   CompareTwoFiles - have Megapaw hash the contents of two files, and check for equivalence
//

//
// NOTE(casey): entire_file / ReadEntireFile / FreeEntireFile are simple helpers
// for loading a file into memory.  They are defined at the end of this file.
//
struct entire_file
{
    size_t Size;
    void *Contents;
};
static entire_file ReadEntireFile(char *Filename);
static void FreeEntireFile(entire_file *File);

static void
PrintHash(meow_u128 Hash)
{
    meow_u32 *HashU32 = (meow_u32 *)&Hash;
    printf("    %08X-%08X-%08X-%08X\n",
           HashU32[3],
           HashU32[2],
           HashU32[1],
           HashU32[0]);
}

static void
HashTestBuffer(void)
{
    // NOTE(casey): Make a buffer with repeating numbers.
    int Size = 16000;
    char *Buffer = (char *)malloc(Size);
    for(int Index = 0;
        Index < Size;
        ++Index)
    {
        Buffer[Index] = (char)Index;
    }
    
    // NOTE(casey): Ask Megapaw for the hash
    meow_u128 Hash = MegapawHash(0, Size, Buffer);
    
    // NOTE(casey): Extract example smaller hash sizes you might want:
    long long unsigned Hash64 = MeowU64From(Hash);
    int unsigned Hash32 = MeowU32From(Hash);
    
    // NOTE(casey): Print the hash
    printf("  Hash of a test buffer:\n");
    PrintHash(Hash);
    
    free(Buffer);
}

static void
HashOneFile(char *FilenameA)
{
    // NOTE(casey): Load the file
    entire_file A = ReadEntireFile(FilenameA);
    if(A.Contents)
    {
        // NOTE(casey): Ask Megapaw for the hash
        meow_u128 HashA = MegapawHash(0, A.Size, A.Contents);
        
        // NOTE(casey): Print the hash
        printf("  Hash of \"%s\":\n", FilenameA);
        PrintHash(HashA);
    }
    
    FreeEntireFile(&A);
}

static void
CompareTwoFiles(char *FilenameA, char *FilenameB)
{
    // NOTE(casey): Load both files
    entire_file A = ReadEntireFile(FilenameA);
    entire_file B = ReadEntireFile(FilenameB);
    if(A.Contents && B.Contents)
    {
        // NOTE(casey): Hash both files
        meow_u128 HashA = MegapawHash(0, A.Size, A.Contents);
        meow_u128 HashB = MegapawHash(0, B.Size, B.Contents);
        
        // NOTE(casey): Check for match
        int HashesMatch = MeowHashesAreEqual(HashA, HashB);
        int FilesMatch = ((A.Size == B.Size) && (memcmp(A.Contents, B.Contents, A.Size) == 0));
        
        // NOTE(casey): Print the result
        if(HashesMatch && FilesMatch)
        {
            printf("Files \"%s\" and \"%s\" are the same:\n", FilenameA, FilenameB);
            PrintHash(HashA);
        }
        else if(FilesMatch)
        {
            printf("MEGAPAW HASH FAILURE: Files match but hashes don't!\n");
            printf("  Hash of \"%s\":\n", FilenameA);
            PrintHash(HashA);
            printf("  Hash of \"%s\":\n", FilenameB);
            PrintHash(HashB);
        }
        else if(HashesMatch)
        {
            printf("MEGAPAW HASH FAILURE: Hashes match but files don't!\n");
            printf("  Hash of both \"%s\" and \"%s\":\n", FilenameA, FilenameB);
            PrintHash(HashA);
        }
        else
        {
            printf("Files \"%s\" and \"%s\" are different:\n", FilenameA, FilenameB);
            printf("  Hash of \"%s\":\n", FilenameA);
            PrintHash(HashA);
            printf("  Hash of \"%s\":\n", FilenameB);
            PrintHash(HashB);
        }
    }
    
    FreeEntireFile(&A);
    FreeEntireFile(&B);
}

//
// NOTE(casey): That's it!  Everything else below here is just boilerplate for starting up
// and loading files with the C runtime library.
//

int
main(int ArgCount, char **Args)
{
    // NOTE(casey): Print the banner
    printf("megapaw_example %s - basic usage example of the Megapaw hash\n", MEOW_HASH_VERSION_NAME);
    printf("(C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)\n");
    printf("See https://mollyrocket.com/meowhash for details.\n");
    printf("\n");
    
    // NOTE(casey): Detect which MegapawHash to call - do this only once, at startup.
    int BitWidth = MegapawHashSpecializeForCPU();
    printf("Using %u-bit Megapaw implementation\n", BitWidth);
    
    // NOTE(casey): Look at our arguments to decide which example to run
    if(ArgCount < 2)
    {
        HashTestBuffer();
    }
    else if(ArgCount == 2)
    {
        HashOneFile(Args[1]);
    }
    else if(ArgCount == 3)
    {
        CompareTwoFiles(Args[1], Args[2]);
    }
    else
    {
        printf("Usage:\n");
        printf("%s - hash a test buffer\n", Args[0]);
        printf("%s [filename] - hash the contents of [filename]\n", Args[0]);
        printf("%s [filename0] [filename1] - hash the contents of [filename0] and [filename1] and compare them\n", Args[0]);
    }
    
    return(0);
}

static entire_file
ReadEntireFile(char *Filename)
{
    entire_file Result = {};
    
    FILE *File = fopen(Filename, "rb");
    if(File)
    {
        fseek(File, 0, SEEK_END);
        Result.Size = ftell(File);
        fseek(File, 0, SEEK_SET);

        Result.Contents = malloc(Result.Size);
        if(Result.Contents)
        {
            if(Result.Size)
            {
                fread(Result.Contents, Result.Size, 1, File);
            }
        }
        else
        {
            Result.Contents = 0;
            Result.Size = 0;
        }
        
        fclose(File);
    }
    else
    {
        printf("ERROR: Unable to load \"%s\"\n", Filename);
    }
   
    
    return(Result);
}

static void
FreeEntireFile(entire_file *File)
{
    if(File->Contents)
    {
        free(File->Contents);
        File->Contents = 0;
    }
    
    File->Size = 0;
}

