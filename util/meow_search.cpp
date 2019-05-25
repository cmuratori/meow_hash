/* ========================================================================

   meow_search.cpp - basic file system Meow hash collision search
   (C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#if _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define MEOW_INCLUDE_TRUNCATIONS 1
#include "meow_test.h"

struct test_file
{
    test_file *Next;
    char *FileName;
    int IsCollision;
};

struct test_value
{
    meow_u128 Hash;
    test_value *Next;
    test_file *FirstFile;
};

struct test
{
    named_hash_type Type;
    meow_u64 CollisionCount;
    test_value *Table[4096];
};

struct test_group
{
    int TestCount;
    test *Tests;

    // NOTE(casey): Statistics
    meow_u64 FileCount;
    meow_u64 ByteCount;
    meow_u64 DuplicateFileCount;
    meow_u64 ChangedFileCount;
    
    // NOTE(casey): Errors
    meow_u64 AccessFailureCount;
    meow_u64 AllocationFailureCount;
    meow_u64 ReadFailureCount;
    
    char *ReportFileName;
    char *RootPath;
};

struct entire_file
{
    size_t Size;
    void *Contents;
};

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

static entire_file
ReadEntireFile(test_group *Group, char *Filename)
{
    entire_file Result = {};
    
    FILE *File = fopen(Filename, "rb");
    if(File)
    {
        fseek(File, 0, SEEK_END);
        Result.Size = ftell(File);
        fseek(File, 0, SEEK_SET);
        
        Result.Contents = aligned_alloc(CACHE_LINE_ALIGNMENT, Result.Size);
        if(Result.Contents)
        {
            if(Result.Size)
            {
                if(fread(Result.Contents, Result.Size, 1, File) == 1)
                {
                    // NOTE(casey): Success!
                }
                else
                {
                    FreeEntireFile(&Result);
                    ++Group->ReadFailureCount;
                }
            }
        }
        else
        {
            Result.Size = 0;
            ++Group->AllocationFailureCount;
        }
        
        fclose(File);
    }
    else
    {
        ++Group->AccessFailureCount;
    }
   
    
    return(Result);
}

static void
WriteReport(test_group *Group, int Completed)
{
    FILE *R = fopen(Group->ReportFileName, "w");
    if(R)
    {
        time_t Time;
        time(&Time);
        tm *TimeInfo = localtime(&Time);
        
        fprintf(R, "meow_search %s results:\n", MEOW_HASH_VERSION_NAME);
        fprintf(R, "    Root: %s\n", Group->RootPath);
        fprintf(R, "    %s: %s", Completed ? "Completed on" : "Progress as of", asctime(TimeInfo));
        fprintf(R, "    Files: %0.0f\n", (double)Group->FileCount);
        fprintf(R, "    Total size: ");
        PrintSize(R, Group->ByteCount, false);
        fprintf(R, "\n");
        fprintf(R, "    Duplicate files: %0.0f\n", (double)Group->DuplicateFileCount);
        fprintf(R, "    Files changed during search: %0.0f\n", (double)Group->ChangedFileCount);
        fprintf(R, "    Access failures: %0.0f\n", (double)Group->AccessFailureCount);
        fprintf(R, "    Allocation failures: %0.0f\n", (double)Group->AllocationFailureCount);
        fprintf(R, "    Read failures: %0.0f\n", (double)Group->ReadFailureCount);
        
        for(int TestIndex = 0;
            TestIndex < Group->TestCount;
            ++TestIndex)
        {
            test *Test = Group->Tests + TestIndex;
            fprintf(R, "    [%s] %s collisions: %0.0f\n", Test->Type.ShortName, Test->Type.FullName, (double)Test->CollisionCount);
            for(int HashSlot = 0;
                HashSlot < ArrayCount(Test->Table);
                ++HashSlot)
            {
                for(test_value *Value = Test->Table[HashSlot];
                    Value;
                    Value = Value->Next)
                {
                    int IsCollision = 0;
                    for(test_file *File = Value->FirstFile;
                        File;
                        File = File->Next)
                    {
                        if(File->IsCollision)
                        {
                            IsCollision = 1;
                            break;
                        }
                    }
                    
                    if(IsCollision)
                    {
                        fprintf(R, "        ");
                        PrintHash(R, Value->Hash);
                        fprintf(R, ":\n");
                        
                        for(test_file *File = Value->FirstFile;
                            File;
                            File = File->Next)
                        {
                            if(File->IsCollision)
                            {
                                fprintf(R, "            %s\n", File->FileName);
                            }
                        }
                    }
                }
            }
        }
        
        fclose(R);
    }
}

static void
IngestFile(test_group *Group, char *FileName)
{
    entire_file File = ReadEntireFile(Group, FileName);
    if(File.Contents)
    {
        ++Group->FileCount;
        Group->ByteCount += File.Size;
        
        int QuickStatus = ((Group->FileCount % 10) == 0);
        if(QuickStatus)
        {
            double Gigabyte = 1024.0*1024.0*1024.0;
            printf("\r%0.0f files, %0.02fgb, %0.0f dupes, %0.0f chng",
                   (double)Group->FileCount,
                   (double)Group->ByteCount / (double)Gigabyte,
                   (double)Group->DuplicateFileCount,
                   (double)Group->ChangedFileCount);
        }
        
        if((Group->FileCount % 1000) == 0)
        {
            WriteReport(Group, false);
        }
        
        int DuplicateFileFound = 0;
        int FileChanged = 0;
        for(int TestIndex = 0;
            TestIndex < Group->TestCount;
            ++TestIndex)
        {
            test *Test = Group->Tests + TestIndex;
            
            meow_u128 Hash = Test->Type.Imp(MeowDefaultSeed, File.Size, File.Contents);
            
            test_value **Slot = &Test->Table[MeowU32From(Hash, 0) % ArrayCount(Test->Table)];
            test_value *Entry = *Slot;
            while(Entry && memcmp(&Entry->Hash, &Hash, sizeof(Hash)))
            {
                Entry = Entry->Next;
            }
            
            int IsCollision = 0;
            if(Entry)
            {
                for(test_file *Check = Entry->FirstFile;
                    Check;
                    Check = Check->Next)
                {
                    entire_file OtherFile = ReadEntireFile(Group, Check->FileName);
                    meow_u128 OtherHash = Test->Type.Imp(MeowDefaultSeed, OtherFile.Size, OtherFile.Contents);
                    if(MeowHashesAreEqual(Hash, OtherHash))
                    {
                        if(OtherFile.Contents &&
                           ((File.Size != OtherFile.Size) ||
                            memcmp(File.Contents, OtherFile.Contents, File.Size)))
                        {
                            Check->IsCollision = 1;
                            IsCollision = 1;
                            ++Test->CollisionCount;
                        }
                        else
                        {
                            DuplicateFileFound = 1;
                        }
                    }
                    else
                    {
                        FileChanged = 1;
                    }
                    FreeEntireFile(&OtherFile);
                }
            }
            else
            {
                Entry = (test_value *)malloc(sizeof(test_value));
                Entry->Hash = Hash;
                Entry->FirstFile = 0;
                Entry->Next = *Slot;
                *Slot = Entry;
            }
            
            test_file *TestFile = (test_file *)malloc(sizeof(test_file));
            TestFile->FileName = FileName;
            TestFile->Next = Entry->FirstFile;
            TestFile->IsCollision = IsCollision;
            Entry->FirstFile = TestFile;
            
            if(QuickStatus && Test->CollisionCount)
            {
                printf(" %s:%u!", Test->Type.ShortName, (int unsigned)Test->CollisionCount);
            }
        }
        
        if(QuickStatus)
        {
            fflush(stdout);
        }
        
        Group->DuplicateFileCount += DuplicateFileFound;
        Group->ChangedFileCount += FileChanged;
    }
    
    FreeEntireFile(&File);
}

static void IngestDirectoriesRecursively(test_group *Group, char *Path);
int main(int ArgCount, char **Args)
{
    int Result = -1;

    InitializeHashesThatNeedInitializers();
    
    if(ArgCount == 3)
    {
        // NOTE(casey): Strip trailing slashes from the input
        char *RootPath = Args[1];
        size_t RootPathLen = strlen(Args[1]);
        while(RootPathLen)
        {
            --RootPathLen;
            if((RootPath[RootPathLen] == '/') ||
               (RootPath[RootPathLen] == '\\'))
            {
                RootPath[RootPathLen] = 0;
            }
            else
            {
                break;
            }
        }
        
        char *ReportFileName = Args[2];
        FILE *ReportFileTest = fopen(ReportFileName, "rb");
        if(!ReportFileTest)
        {
            // NOTE(casey): Prepare the test group
            test Tests[ArrayCount(NamedHashTypes)] = {};
            for(int TestIndex = 0;
                TestIndex < ArrayCount(Tests);
                ++TestIndex)
            {
                Tests[TestIndex].Type = NamedHashTypes[TestIndex];
            }
            
            test_group Group = {};
            Group.TestCount = ArrayCount(Tests);
            Group.Tests = Tests;
            Group.ReportFileName = ReportFileName;
            Group.RootPath = RootPath;
            
            // NOTE(casey): Print the banner
            time_t Time;
            time(&Time);
            tm *TimeInfo = localtime(&Time);
            
            printf("meow_search %s began at %s", MEOW_HASH_VERSION_NAME, asctime(TimeInfo));
            printf("Root: %s\n", RootPath);
            printf("Hash types:\n");
            for(int TestIndex = 0;
                TestIndex < Group.TestCount;
                ++TestIndex)
            {
                test *Test = Group.Tests + TestIndex;
                printf("    %s = %s\n", Test->Type.ShortName, Test->Type.FullName);
            }
            
            // NOTE(casey): Run the search
            IngestDirectoriesRecursively(&Group, RootPath);
            printf("\n");
            printf("meow_search complete.\n");
            
            // NOTE(casey): Report the results
            WriteReport(&Group, true);
            
            // NOTE(casey): Prepare a result code based on the collision count
            Result = (int)Group.Tests[MEOW_HASH_TEST_INDEX_128].CollisionCount;
        }
        else
        {
            printf("ERROR: %s already exists.  Please specify a different report filename.\n", ReportFileName);
            fclose(ReportFileTest);
        }
    }
    else
    {
        printf("Usage: %s <directory to search recursively> <report filename to write>\n", Args[0]);
    }
    
    return(Result);
}

//
// NOTE(casey) Platform-specific directory walking
//

static char *
AllocPath(char *A, char *B)
{
    size_t ACount = strlen(A);
    size_t BCount = strlen(B);
    size_t Size = ACount + BCount + 2;
    
    char *Result = (char *)malloc(Size);
    memcpy(Result, A, ACount);
    memcpy(Result + ACount + 1, B, BCount);
    Result[ACount] = '/';
    Result[Size - 1] = 0;
    
    return(Result);
}

static void
DeallocPath(char *A)
{
    if(A)
    {
        free(A);
    }
}

#if _WIN32

static void
IngestDirectoriesRecursively(test_group *Group, char *Path)
{
    char *Wildcard = AllocPath(Path, (char *)"*");
    
    WIN32_FIND_DATAA FindData;
    HANDLE SearchHandle = FindFirstFileExA(Wildcard, FindExInfoBasic, &FindData,
                                           FindExSearchNameMatch, 0, FIND_FIRST_EX_LARGE_FETCH);
    if(SearchHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            char *Stem = FindData.cFileName;
            if(strcmp(Stem, ".") && strcmp(Stem, ".."))
            {
                char *EntryName = AllocPath(Path, Stem);
                if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    IngestDirectoriesRecursively(Group, EntryName);
                }
                else
                {
                    IngestFile(Group, EntryName);
                }
            }
        } while(FindNextFileA(SearchHandle, &FindData));
        
        FindClose(SearchHandle);
    }
    
    DeallocPath(Wildcard);
}

#else

static void
IngestDirectoriesRecursively(test_group *Group, char *Path)
{
    DIR *DirHandle = opendir(Path);
    if(DirHandle)
    {
        for(dirent *Entry = readdir(DirHandle);
            Entry;
            Entry = readdir(DirHandle))
        {
            // NOTE(casey): We intentionally never free these, because they are
            // used in the permanent structure.
            char *Stem = Entry->d_name;
            char *EntryName = AllocPath(Path, Stem);
            if(strcmp(Stem, ".") && strcmp(Stem, ".."))
            {
                if(Entry->d_type == DT_UNKNOWN)
                {
                    ++Group->AccessFailureCount;
                }
                else if(Entry->d_type == DT_DIR)
                {
                    IngestDirectoriesRecursively(Group, EntryName);
                }
                else if(Entry->d_type == DT_REG)
                {
                    IngestFile(Group, EntryName);
                }
            }
        }
        
        closedir(DirHandle);
    }
}

#endif
