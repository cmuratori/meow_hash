/* ========================================================================

   meow_example.cpp - basic usage example of the Meow hash
   (C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

//
// NOTE(casey): Step 1 - include the meow_hash header for your platform
//

#include "meow_hash_x64_aesni.h"

//
// NOTE(casey): Step 2 - use the Meow hash in a variety of ways!
//
// Example functions below:
//   PrintHash - how to print a Meow hash to stdout, from highest-order 32-bits to lowest
//   HashTestBuffer - how to have Meow hash a buffer of data
//   HashOneFile - have Meow hash the contents of a file
//   CompareTwoFiles - have Meow hash the contents of two files, and check for equivalence
//

//
// NOTE(casey): entire_file / ReadEntireFile / FreeEntireFile are simple helpers
// for loading a file into memory.  They are defined at the end of this file.
//
typedef struct entire_file
{
    size_t Size;
    void *Contents;
} entire_file;
static entire_file ReadEntireFile(char *Filename);
static void FreeEntireFile(entire_file *File);

static void
PrintHash(meow_u128 Hash)
{
    printf("    %08X-%08X-%08X-%08X\n",
           MeowU32From(Hash, 3),
           MeowU32From(Hash, 2),
           MeowU32From(Hash, 1),
           MeowU32From(Hash, 0));
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
    
    // NOTE(casey): Ask Meow for the hash
    meow_u128 Hash = MeowHash(MeowDefaultSeed, Size, Buffer);
    
    // NOTE(casey): Extract example smaller hash sizes you might want:
    long long unsigned Hash64 = MeowU64From(Hash, 0);
    int unsigned Hash32 = MeowU32From(Hash, 0);
    
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
        // NOTE(casey): Ask Meow for the hash
        meow_u128 HashA = MeowHash(MeowDefaultSeed, A.Size, A.Contents);
        
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
        meow_u128 HashA = MeowHash(MeowDefaultSeed, A.Size, A.Contents);
        meow_u128 HashB = MeowHash(MeowDefaultSeed, B.Size, B.Contents);
        
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
            printf("MEOW HASH FAILURE: Files match but hashes don't!\n");
            printf("  Hash of \"%s\":\n", FilenameA);
            PrintHash(HashA);
            printf("  Hash of \"%s\":\n", FilenameB);
            PrintHash(HashB);
        }
        else if(HashesMatch)
        {
            printf("MEOW HASH FAILURE: Hashes match but files don't!\n");
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
    printf("meow_example %s - basic usage example of the Meow hash\n", MEOW_HASH_VERSION_NAME);
    printf("(C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)\n");
    printf("See https://mollyrocket.com/meowhash for details.\n");
    printf("\n");
    
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
    entire_file Result = {0};
    
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

