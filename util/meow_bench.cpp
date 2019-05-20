/* ========================================================================

   meow_bench.cpp - basic RDTSC-based benchmark for the Meow hash
   (C) Copyright 2018-2019 by Molly Rocket, Inc. (https://mollyrocket.com)
   
   See https://mollyrocket.com/meowhash for details.
   
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef __aarch64__
// NOTE(mmozeiko): On ARM you normally cannot access cycle counter from user-space.
// Download & build following kernel module that enables access to PMU cycle counter
// from user-space code: https://github.com/zhiyisun/enable_arm_pmu
#include <stdint.h>
#include "enable_arm_pmu/armpmu_lib.h"
#define __rdtsc() read_pmu()
#define __rdtscp(x) read_pmu()
#endif

#ifndef _WIN32
static int unsigned
_rotl(int unsigned Value, int Count)
{
    int unsigned Result = (Value << Count) | (Value >> (32 - Count));
    return Result;
}
#endif

#ifdef _MSC_VER
#ifdef __clang__
#if MEOW_INCLUDE_OTHER_HASHES
#define CPUID(A, B) __cpuid(B, A[0], A[1], A[2], A[3])
#else
#define CPUID(...) __cpuid(__VA_ARGS__)
#endif
#else
#define CPUID(...) __cpuid(__VA_ARGS__)
#endif
#else
#if __i386__
#define CPUID(Array, Value) __asm__ __volatile__("cpuid" : : : "eax", "ebx", "ecx", "edx")
#elif __x86_64__
#define CPUID(Array, Value) __asm__ __volatile__("cpuid" : : : "rax", "rbx", "rcx", "rdx")
#else
#define CPUID(Array, Value)
#endif
#endif

#include "meow_test.h"

#define Kb(x) ((meow_u64)(x)*(meow_u64)1024)
#define Mb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024)
#define Gb(x) ((meow_u64)(x)*(meow_u64)1024*(meow_u64)1024*(meow_u64)1024)

struct test_results
{
    int unsigned HashType;
    
    meow_u64 Size;
    meow_u64 MinClocks;
    meow_u64 ExpClocks;
    
    double MinBPC;
    double ExpBPC;
};

struct input_size_test
{
    meow_u64 ClockCount;
    meow_u64 ClockAccum;
    meow_u64 ClockExp;
    meow_u64 ClockMin;
    
    // NOTE(casey): We use a fake result target for writing in hopes of convincing the optimizer
    // that the computed hashes are actually used, and so won't be optimized out.
    meow_u128 FakeSlot;
    
    meow_u64 Size;
};

#ifdef __aarch64__
#define MAX_SIZE_TO_TEST Gb(1)
#else
#define MAX_SIZE_TO_TEST Gb(2)
#endif
#define SIZE_TYPE_COUNT 64
#define SIZE_COUNT_PER_BATCH 16
struct input_size_tests
{
    meow_u64 MaxClockCount;
    meow_u64 RunsPerHashImplementation;
    int unsigned ClocksPerAvg;
    input_size_test Sizes[SIZE_COUNT_PER_BATCH];

    int unsigned ResultCount;
    test_results Results[SIZE_COUNT_PER_BATCH * SIZE_TYPE_COUNT * ArrayCount(NamedHashTypes)];
    
    meow_u64 SizeSeries;
    char Name[64];
};

static void
FuddleBuffer(meow_u64 Size, void *Buffer, int Seed)
{
    meow_u128 *Dest = (meow_u128 *)Buffer;
    meow_u128 Stamp = {};
    *(meow_u64 *)&Stamp = Seed;
    for(meow_u64 Index = 0;
        Index < (Size >> 4);
        ++Index)
    {
        *Dest++ = Stamp;
    }
    
    meow_u8 *Overhang = (meow_u8 *)Dest;
    for(meow_u64 Index = 0;
        Index < (Size & 0xF);
        ++Index)
    {
        Overhang[Index] = 13*Index + Seed;
    }
}

static int
ResultCompare(const void *AInit, const void *BInit)
{
    test_results *A = (test_results *)AInit;
    test_results *B = (test_results *)BInit;
    
    int Result = 0;
    if(A->Size > B->Size)
    {
        Result = 1;
    }
    else if(A->Size < B->Size)
    {
        Result = -1;
    }
    else if(A->ExpClocks > B->ExpClocks)
    {
        Result = 1;
    }
    else if(A->ExpClocks < B->ExpClocks)
    {
        Result = -1;
    }
    
    return(Result);
}

static test_results *
CommitResults(int unsigned HashType, input_size_test *Test, input_size_tests *DestTests)
{
    test_results *Results = 0;
    
    if(DestTests->ResultCount < ArrayCount(DestTests->Results))
    {
        Results = DestTests->Results + DestTests->ResultCount++;
        Results->HashType = HashType;
        Results->Size = Test->Size;
        Results->MinClocks = Test->ClockMin;
        Results->ExpClocks = Test->ClockExp;
        
        Results->ExpBPC = 0.0f;
        if(Results->ExpClocks)
        {
            Results->ExpBPC = (double)Test->Size / (double)Results->ExpClocks;
        }
        
        Results->MinBPC = 0.0f;
        if(Results->MinClocks)
        {
            Results->MinBPC = (double)Test->Size / (double)Results->MinClocks;
        }
    }
    
    return(Results);
}

static int unsigned
Random(meow_u64 *Series)
{
    meow_u64 TestRand = *Series;
    
    // NOTE(casey): This is a XorShift64* (LCG) followed by an O'Neill random rotation (PCG)
    TestRand ^= TestRand >> 12;
    TestRand ^= TestRand << 25;
    TestRand ^= TestRand >> 27;
    
    int unsigned Result = _rotl((int unsigned)((TestRand ^ (TestRand>>18))>>27), (int)(TestRand >> 59));
    *Series = TestRand * 2685821657736338717LL;
    
    return(Result);
}

static void
InitializeTests(input_size_tests *Tests, int unsigned SizeType, meow_u64 MaxClockCount)
{
    // NOTE(casey): Each range has a variable amount of "extension" into the next range to ensure that
    // there is enough overlap for relatively continuous graphing that (hopefully) doesn't miss
    // anything important.
    
    char *NameBase = (char *)"(unknown)";
    meow_u64 Start = 1;
    meow_u64 End = 1024;
    meow_u64 Divisor = 1;
    int unsigned ClocksPerAvg = 100;
    if(SizeType < 16)
    {
        // NOTE(casey): 1b - 1024b
        NameBase = (char *)"Tiny Input";
        Divisor = 1;
        
        Start = 0;
        End = 4096;
    }
    else if(SizeType < 30)
    {
        // NOTE(casey): 1k - 64k
        NameBase = (char *)"Small Input";
        Divisor = 50;
        
        Start = Kb(1);
        End = Kb(512);
    }
    else if(SizeType < 42)
    {
        // NOTE(casey): 64k - 1mb
        NameBase = (char *)"Medium Input";
        Divisor = 200;
        ClocksPerAvg = 10;
        
        Start = Kb(64);
        End = Mb(4);
    }
    else if(SizeType < 60)
    {
        // NOTE(casey): 1mb - 512mb
        NameBase = (char *)"Large Input";
        Divisor = 10000;
        ClocksPerAvg = 5;
        
        Start = Mb(1);
        End = Mb(48);
    }
    else if(SizeType < 64)
    {
        // NOTE(casey): 512mb - 2gb
        NameBase = (char *)"Giant Input";
        Divisor = 500000;
        ClocksPerAvg = 1;
        
        Start = Mb(32);
        End = MAX_SIZE_TO_TEST;
    }
    
    meow_u64 Range = End - Start;
    for(int Index = 0;
        Index < ArrayCount(Tests->Sizes);
        ++Index)
    {
        Tests->Sizes[Index].Size = Start + (Random(&Tests->SizeSeries) % Range);
    }
    
    sprintf(Tests->Name, "%s%u", NameBase, SizeType);
    Tests->ClocksPerAvg = ClocksPerAvg;
    Tests->MaxClockCount = (MaxClockCount / Divisor);
    Tests->RunsPerHashImplementation = (ArrayCount(Tests->Sizes) * Tests->MaxClockCount);
    
#if MEOW_TEST_BENCH_QUICK
    Tests->RunsPerHashImplementation /= 32;
#endif
}

static void
PrintLeaderboard(input_size_tests *Tests, FILE *Stream)
{
    fprintf(Stream, "Leaderboard:\n");
    
    qsort(Tests->Results, Tests->ResultCount, sizeof(Tests->Results[0]), ResultCompare);
    
    int unsigned ResultIndex = 0;
    while(ResultIndex < Tests->ResultCount)
    {
        test_results *BestResults = Tests->Results + ResultIndex;
        
        meow_u64 CurSize = BestResults->Size;
        meow_u64 ExpClocks = BestResults->ExpClocks;
        meow_u64 MaxClocks = ExpClocks; // + (ExpClocks/ 100);
        
        fprintf(Stream, "    ");
        PrintSize(Stream, BestResults->Size, true);
        fprintf(Stream, ": %10.0f (%6.03f bytes/cycle) - ",
                (double)BestResults->ExpClocks, (double)BestResults->ExpBPC);
        
        int TieCount = 0;
        while(ResultIndex < Tests->ResultCount)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->Size == CurSize)
            {
                if(Results->ExpClocks <= MaxClocks)
                {
                    named_hash_type Type = NamedHashTypes[Results->HashType];
                    
                    if(TieCount)
                    {
                        fprintf(Stream, ", ");
                    }
                    
                    fprintf(Stream, "%s", Type.FullName);
                    
                    ++TieCount;
                }
                ++ResultIndex;
            }
            else
            {
                break;
            }
        }
        if(TieCount > 1)
        {
            fprintf(Stream, " (%d-way tie)", TieCount);
        }
        fprintf(Stream, "\n");
    }
}
        
void
WriteCSV(input_size_tests *Tests, char *CSVFileName)
{
    int unsigned TypeCount = ArrayCount(NamedHashTypes);

    FILE *CSV = fopen(CSVFileName, "w");
    if(CSV)
    {
        meow_u64 LastSize = 0;
        fprintf(CSV, "Input");
        for(int ResultIndex = 0;
            ResultIndex < Tests->ResultCount;
            ++ResultIndex)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->Size != LastSize)
            {
                LastSize = Results->Size;
                fprintf(CSV, ",%.0f", (double)LastSize);
            }
        }
        fprintf(CSV, "\n");
        
        LastSize = 0;
        for(int TypeIndex = 0;
            TypeIndex < TypeCount;
            ++TypeIndex)
        {
            named_hash_type Type = NamedHashTypes[TypeIndex];
            fprintf(CSV, "%s", Type.FullName);
            for(int ResultIndex = 0;
                ResultIndex < Tests->ResultCount;
                ++ResultIndex)
            {
                test_results *Results = Tests->Results + ResultIndex;
                if((Results->HashType == TypeIndex) &&
                   (Results->Size != LastSize))
                {
                    LastSize = Results->Size;
                    fprintf(CSV, ",%f", Results->ExpBPC);
                }
            }
            fprintf(CSV, "\n");
        }
        fprintf(CSV, "\n");
        fprintf(CSV, "\n");
        for(int TypeIndex = 0;
            TypeIndex < TypeCount;
            ++TypeIndex)
        {
            named_hash_type Type = NamedHashTypes[TypeIndex];
            fprintf(CSV, "%s", Type.FullName);
            for(int ResultIndex = 0;
                ResultIndex < Tests->ResultCount;
                ++ResultIndex)
            {
                test_results *Results = Tests->Results + ResultIndex;
                if((Results->HashType == TypeIndex) &&
                   (Results->Size != LastSize))
                {
                    LastSize = Results->Size;
                    fprintf(CSV, ",%f", Results->MinBPC);
                }
            }
            fprintf(CSV, "\n");
        }
        fprintf(CSV, "\n");
        fclose(CSV);
    }
    else
    {
        fprintf(stderr, "    (unable to open %s for writing)\n",
                CSVFileName);
    }
}

void
WriteHTML(input_size_tests *Tests, char *HTMLFileName)
{
    int unsigned TypeCount = ArrayCount(NamedHashTypes);

    // NOTE(casey): Dear C++ committee: how about you define the string as writable IF I TOLD YOU IT WAS WRITABLE
    // instead of making it constant?  Oh, that's right - because you don't care what the programmer wants,
    // you're always right about how code should work.  My bad.  So I get to type the word "const" three times
    // for no reason whatsoever.  Nice work!  You really saved the day.
    char const *CrappyColorTable[] =
    {
        "#477ea0",
        "#84ab3c",
        "#50c76d",
        "#d8823a",
        "#da5bd5",
        "#d63636",
        "#897cd0",
        "#949494",
        "#af5f9d",
        "#2ac4c5",
        "#3b17c1",
        "#ff0000", // TODO(casey): Real color here
    };
    
    FILE *Out = fopen(HTMLFileName, "w");
    if(Out)
    {
        fprintf(Out, "<!DOCTYPE html><html lang='en' itemscope itemtype='https://schema.org/Article'><head>\n");
        fprintf(Out, "</head>\n");
        fprintf(Out, "<body>\n");
        
        double MaxSizeCoord = log(1.0f + MAX_SIZE_TO_TEST);
        double MaxSpeedCoord = 16.0; // TODO(casey): Should probably scan for this, eventually
        
        {
            meow_u64 LastSize = -1;
            fprintf(Out, "<svg id='Graph' style='width:100%%;height:50vh;user-select:none;' viewBox='0 0 %f %f'>\n", MaxSizeCoord, MaxSpeedCoord);

            fprintf(Out, "<g id='ViewXForm'>\n");
            double XLine = 1.0f;
            for(int XIndex = 0;
                ;
                ++XIndex)
            {
                if(XIndex)
                {
                    double XCoord = log(1.0f + XLine);
                    if(XCoord < MaxSizeCoord)
                    {
                        char const *Color = "#eeeeee";
                        if((XIndex % 4) == 0)
                        {
                            Color = "#888888";
                            
                            fprintf(Out, "<text x='%f' y='%f' fill='%s' font-size='0.5' alignment-baseline='baseline'>", XCoord, MaxSpeedCoord, Color);
                            PrintSize(Out, XLine, false, false);
                            fprintf(Out, "</text>\n");
                        }
                        
                        fprintf(Out, "<line x1='%f' y1='0' x2='%f' y2='%f' vector-effect='non-scaling-stroke' stroke='%s' stroke-width='1.0' />\n",
                                XCoord, XCoord, MaxSpeedCoord, Color);
                        
                        XLine *= 2.0f;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            
            double YLine = 0.0f;
            for(;;)
            {
                double YCoord = (MaxSpeedCoord - YLine);
                if(YLine < MaxSpeedCoord)
                {
                    char const *Color = "#eeeeee";
                    if((YLine != 0.0) && (fmod(YLine, 4.0) == 0.0))
                    {
                        Color = "#888888";
                        
                        char Label[32];
                        sprintf(Label, "%.0fb/c", YLine);
                        fprintf(Out, "<text x='0' y='%f' fill='#888888' font-size='0.5' alignment-baseline='hanging'>%s</text>\n", YCoord, Label);
                    }
                    
                    fprintf(Out, "<line x1='0' y1='%f' x2='%f' y2='%f' vector-effect='non-scaling-stroke' stroke='%s' stroke-width='1.0' />\n",
                            YCoord, MaxSizeCoord, YCoord, Color);
                    
                    YLine += 1.0f;
                }
                else
                {
                    break;
                }
            }

            fprintf(Out, "<line id='size_line' x1='0' y1='0' x2='0' y2='%f' vector-effect='non-scaling-stroke' stroke='#ffaaaa' stroke-width='3.0' />\n", MaxSpeedCoord);
            fprintf(Out, "<line id='speed_line' x1='0' y1='0' x2='%f' y2='0' vector-effect='non-scaling-stroke' stroke='#ffaaaa' stroke-width='3.0' />\n", MaxSizeCoord);

            for(int TypeIndex = 0;
                TypeIndex < TypeCount;
                ++TypeIndex)
            {
                named_hash_type Type = NamedHashTypes[TypeIndex];
                
                fprintf(Out, "<path class='%s_exp gel' vector-effect='non-scaling-stroke' d='", Type.ShortName);
                char Char = 'M';
                char const *Color = CrappyColorTable[TypeIndex%ArrayCount(CrappyColorTable)];
                for(int ResultIndex = 0;
                    ResultIndex < Tests->ResultCount;
                    ++ResultIndex)
                {
                    test_results *Results = Tests->Results + ResultIndex;
                    if((Results->HashType == TypeIndex) &&
                       (Results->Size != LastSize))
                    {
                        LastSize = Results->Size;
                        
                        double SizeCoord = log(1.0f + Results->Size);
                        double SpeedCoord = (MaxSpeedCoord - Results->ExpBPC);
                        fprintf(Out, "%c %f %f ", Char, SizeCoord, SpeedCoord);
                        Char = 'L';
                    }
                }
                fprintf(Out, "' stroke-width='1' fill='none' stroke='%s' />\n", Color);
                
                fprintf(Out, "<path class='%s_min gel' vector-effect='non-scaling-stroke' d='", Type.ShortName);
                Char = 'M';
                for(int ResultIndex = 0;
                    ResultIndex < Tests->ResultCount;
                    ++ResultIndex)
                {
                    test_results *Results = Tests->Results + ResultIndex;
                    if((Results->HashType == TypeIndex) &&
                       (Results->Size != LastSize))
                    {
                        LastSize = Results->Size;
                        
                        double SizeCoord = log(1.0f + Results->Size);
                        double SpeedCoord = (MaxSpeedCoord - Results->MinBPC);
                        fprintf(Out, "%c %f %f ", Char, SizeCoord, SpeedCoord);
                        Char = 'L';
                    }
                }
                fprintf(Out, "' stroke-width='1' fill='none' stroke='%s' />\n", Color);
            }
            fprintf(Out, "</g>\n");
            
            fprintf(Out, "</svg>\n");
        }
        
        {
            fprintf(Out, "<div style='font-family:sans-serif;overflow:auto;height:45vh;'>\n");
            fprintf(Out, "<table style='border-spacing:0;'>\n");
            
            // NOTE(casey): Hash names
            fprintf(Out, "<tr style='text-align:center'>");
            fprintf(Out, "<td onclick='PickHash(0, 0, 0, %f)' style='padding:.5rem;'></td>", MaxSpeedCoord);
            for(int TypeIndex = 0;
                TypeIndex < TypeCount;
                ++TypeIndex)
            {
                char const *Color = CrappyColorTable[TypeIndex%ArrayCount(CrappyColorTable)];
                named_hash_type Type = NamedHashTypes[TypeIndex];
                fprintf(Out, "<td colspan=2 style='padding:.5rem;color:%s;'>%s</td>", Color, Type.FullName);
            }
            fprintf(Out, "</tr>\n");

            fprintf(Out, "<tr style='background:#888888;color:#ffffff;text-align:center;'>");
            fprintf(Out, "<td onclick='PickHash(0, 0, 0, %f)'></td>", MaxSpeedCoord);
            for(int TypeIndex = 0;
                TypeIndex < TypeCount;
                ++TypeIndex)
            {
                named_hash_type Type = NamedHashTypes[TypeIndex];
                fprintf(Out, "<td onclick='PickHash(0, \"%s_min\", 0, %f)' style='padding:.5rem'>Fastest</td><td onclick='PickHash(0, \"%s_exp\", 0, %f)' style='padding:.5rem'>Expected</td>", Type.ShortName, MaxSpeedCoord, Type.ShortName, MaxSpeedCoord);
            }
            fprintf(Out, "</tr>\n");

            // NOTE(casey): Hash results
            int RowIndex = 0;
            int BaseResultIndex = 0;
            while(BaseResultIndex < Tests->ResultCount)
            {
                test_results *BaseResult = Tests->Results + BaseResultIndex;
                meow_u64 SizeMatch = BaseResult->Size;
                
                fprintf(Out, "<tr class='row%.0f' style='background:%s;text-align:center;'>", (double)SizeMatch, (RowIndex % 2) ? "#f7f7f7" : "#e6e6e6");
                fprintf(Out, "<td onclick='PickHash(\"row%.0f\", 0, %f, %f)' style='padding:.5rem;text-align:right;'>", (double)SizeMatch, log(1.0 + SizeMatch), MaxSpeedCoord);
                PrintSize(Out, SizeMatch, false);
                fprintf(Out, "</td>");
                for(int TypeIndex = 0;
                    TypeIndex < TypeCount;
                    ++TypeIndex)
                {
                    named_hash_type Type = NamedHashTypes[TypeIndex];
                    
                    double Min = 0;
                    double Exp = 0;
                    for(int ResultIndex = BaseResultIndex;
                       ResultIndex < Tests->ResultCount;
                       ++ResultIndex)
                    {
                        test_results *Results = Tests->Results + ResultIndex;
                        
                        if(Results->Size != SizeMatch)
                        {
                            break;
                        }
                        
                        if(Results->HashType == TypeIndex)
                        {
                            Min = Results->MinBPC;
                            Exp = Results->ExpBPC;
                        }
                    }
                    
                    fprintf(Out, "<td onclick='PickHash(\"row%.0f\", \"%s_min\", %f, %f)' class='%s_min' style='padding:.5rem;'>%f</td><td onclick='PickHash(\"row%.0f\", \"%s_exp\", %f, %f)' class='%s_exp' style='padding:.5rem;'>%f</td>",
                            (double)SizeMatch, Type.ShortName, log(1.0 + SizeMatch), (MaxSpeedCoord - Min),
                            Type.ShortName, Min,
                            (double)SizeMatch, Type.ShortName, log(1.0 + SizeMatch), (MaxSpeedCoord - Exp),
                            Type.ShortName, Exp);
                }

                while(BaseResultIndex < Tests->ResultCount)
                {
                    test_results *Results = Tests->Results + BaseResultIndex;
                    if(Results->Size != SizeMatch)
                    {
                        break;
                    }
                    
                    ++BaseResultIndex;
                }

                fprintf(Out, "</tr>\n");
                
                ++RowIndex;
            }
            
            fprintf(Out, "</table>\n");

            // onwheel='GraphWheel()' ondrag='GraphDrag()'
            
            fprintf(Out, "<script>\n");
            fprintf(Out, "var Scale = 1.0;\n");
            fprintf(Out, "var TranslateX = 0.0;\n");
            fprintf(Out, "var TranslateY = 0.0;\n");
            fprintf(Out, "var DragAnchorX = 0.0;\n");
            fprintf(Out, "var DragAnchorY = 0.0;\n");
            fprintf(Out, "var HighCol = 0;\n");
            fprintf(Out, "var HighRow = 0;\n");
            fprintf(Out, "function PickHash(RowID, ColID, x, y)\n");
            fprintf(Out, "{\n");
            fprintf(Out, "    if(HighCol) {HighCol.remove();HighCol = 0;}\n");
            fprintf(Out, "    if(HighRow) {HighRow.remove();HighRow = 0;}\n");
            fprintf(Out, "    if(ColID)\n");
            fprintf(Out, "    {\n");
            fprintf(Out, "        HighCol = document.createElement('style');\n");
            fprintf(Out, "        document.head.appendChild(HighCol);\n");
            fprintf(Out, "        HighCol.innerHTML = '.gel {opacity:0.25;} .' + ColID + '{color:#aa0000;stroke-width:3;opacity:1.0;}';\n");
            fprintf(Out, "    }\n");
            fprintf(Out, "    if(RowID)\n");
            fprintf(Out, "    {\n");
            fprintf(Out, "        HighRow = document.createElement('style');\n");
            fprintf(Out, "        document.head.appendChild(HighRow);\n");
            fprintf(Out, "        HighRow.innerHTML = '.' + RowID + '{color:#aa0000;}';\n");
            fprintf(Out, "    }\n");
            fprintf(Out, "    SpeedLine = document.getElementById('speed_line');\n");
            fprintf(Out, "    SpeedLine.setAttribute('y1', '' + y + '');\n");
            fprintf(Out, "    SpeedLine.setAttribute('y2', '' + y + '');\n");
            fprintf(Out, "    SizeLine = document.getElementById('size_line');\n");
            fprintf(Out, "    SizeLine.setAttribute('x1', '' + x + '');\n");
            fprintf(Out, "    SizeLine.setAttribute('x2', '' + x + '');\n");
            fprintf(Out, "}\n");
            fprintf(Out, "function GraphWheel(e)\n");
            fprintf(Out, "{\n");
            fprintf(Out, "    Scale *= (1.0 - 0.001*e.deltaY);\n");
            fprintf(Out, "    Graph = document.getElementById('ViewXForm');\n");
            fprintf(Out, "    Graph.setAttributeNS(null, 'transform', 'scale(' + Scale + ') translate(' + TranslateX + ', ' + TranslateY + ')');\n");
            fprintf(Out, "}\n");
            fprintf(Out, "function GraphDrag(e)\n");
            fprintf(Out, "{\n");
            fprintf(Out, "    var dX = e.pageX - DragAnchorX;\n");
            fprintf(Out, "    var dY = e.pageY - DragAnchorY;\n");
            fprintf(Out, "    DragAnchorX = e.pageX;\n");
            fprintf(Out, "    DragAnchorY = e.pageY;\n");
            fprintf(Out, "    if(e.buttons == 1)\n");
            fprintf(Out, "    {\n");
            fprintf(Out, "        TranslateX += Scale*0.01*dX;\n");
            fprintf(Out, "        TranslateY += Scale*0.01*dY;\n");
            fprintf(Out, "        Graph = document.getElementById('ViewXForm');\n");
            fprintf(Out, "        Graph.setAttributeNS(null, 'transform', 'scale(' + Scale + ') translate(' + TranslateX + ', ' + TranslateY + ')');\n");
            fprintf(Out, "    }\n");
            fprintf(Out, "}\n");
            fprintf(Out, "function GraphDown(e)\n");
            fprintf(Out, "{\n");
            fprintf(Out, "    DragAnchorX = e.pageX;\n");
            fprintf(Out, "    DragAnchorY = e.pageY;\n");
            fprintf(Out, "}\n");
            fprintf(Out, "document.getElementById('Graph').addEventListener('wheel', GraphWheel);\n");
            fprintf(Out, "document.getElementById('Graph').addEventListener('mousedown', GraphDown);\n");
            fprintf(Out, "document.getElementById('Graph').addEventListener('mousemove', GraphDrag);\n");
            fprintf(Out, "</script>\n");
        }
        
#if 0
        meow_u64 LastSize = 0;
        fprintf(Out, "Input");
        for(int ResultIndex = 0;
            ResultIndex < Tests->ResultCount;
            ++ResultIndex)
        {
            test_results *Results = Tests->Results + ResultIndex;
            if(Results->Size != LastSize)
            {
                LastSize = Results->Size;
                fprintf(Out, ",%.0f", (double)LastSize);
            }
        }
        fprintf(Out, "\n");
        
        LastSize = 0;
        for(int TypeIndex = 0;
            TypeIndex < TypeCount;
            ++TypeIndex)
        {
            named_hash_type Type = NamedHashTypes[TypeIndex];
            fprintf(Out, "%s", Type.FullName);
            for(int ResultIndex = 0;
                ResultIndex < Tests->ResultCount;
                ++ResultIndex)
            {
                test_results *Results = Tests->Results + ResultIndex;
                if((Results->HashType == TypeIndex) &&
                   (Results->Size != LastSize))
                {
                    LastSize = Results->Size;
                    fprintf(Out, ",%f", Results->ExpBPC);
                }
            }
            fprintf(Out, "\n");
        }
        fprintf(Out, "\n");
        fprintf(Out, "\n");
        for(int TypeIndex = 0;
            TypeIndex < TypeCount;
            ++TypeIndex)
        {
            named_hash_type Type = NamedHashTypes[TypeIndex];
            fprintf(Out, "%s", Type.FullName);
            for(int ResultIndex = 0;
                ResultIndex < Tests->ResultCount;
                ++ResultIndex)
            {
                test_results *Results = Tests->Results + ResultIndex;
                if((Results->HashType == TypeIndex) &&
                   (Results->Size != LastSize))
                {
                    LastSize = Results->Size;
                    fprintf(Out, ",%f", Results->MinBPC);
                }
            }
            fprintf(Out, "\n");
        }
#endif

        fprintf(Out, "</body></html>\n");
        
        fclose(Out);
    }
    else
    {
        fprintf(stderr, "    (unable to open %s for writing)\n",
                HTMLFileName);
    }
}

int
main(int ArgCount, char **Args)
{
#if __aarch64__
    enable_pmu(0x008);
#endif
    
    InitializeHashesThatNeedInitializers();
    
    char *HTMLFileName = 0;
    char *CSVFileName = 0;
    if(ArgCount == 2)
    {
        size_t AllocSize = strlen(Args[1]) + 16;
        
        CSVFileName = (char *)aligned_alloc(16, AllocSize);
        HTMLFileName = (char *)aligned_alloc(16, AllocSize);
        
        sprintf(CSVFileName, "%s.csv", Args[1]);
        sprintf(HTMLFileName, "%s.html", Args[1]);
    }
    
    fprintf(stdout, "\n");
    fprintf(stdout, "meow_bench %s - basic RDTSC-based benchmark for the Meow hash\n", MEOW_HASH_VERSION_NAME);
    fprintf(stdout, "    See https://mollyrocket.com/meowhash for details\n");
    fprintf(stdout, "    WARNING: Counts are NOT accurate if CPU power throttling is enabled\n");
    fprintf(stdout, "             (You must turn it off in your OS if you haven't yet!)\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Versions compiled into this benchmark:\n");
    
    meow_u64 MaxClockCount = (meow_u64)10000000;
    // NOTE(mmozeiko): Test memory should be aligned to 16 because it contains member with SIMD type
    // which means that compiler can potentially issue aligned store to this SIMD member
    input_size_tests *Tests = (input_size_tests *)aligned_alloc(16, sizeof(input_size_tests));
    Tests->SizeSeries = 123456789;
    Tests->ResultCount = 0;
    
    int unsigned TypeCount = ArrayCount(NamedHashTypes);
    for(int unsigned TypeIndex = 0;
        TypeIndex < TypeCount;
        ++TypeIndex)
    {
        named_hash_type Type = NamedHashTypes[TypeIndex];
        fprintf(stdout, "    %d. %s\n", TypeIndex + 1, Type.FullName);
    }
    fprintf(stdout, "\n");
    
    void *Buffer = aligned_alloc(CACHE_LINE_ALIGNMENT, MAX_SIZE_TO_TEST);
    if(Buffer)
    {
        int unsigned SizePattern[] =
        {
            16,  0, 30, 42, 60,
            17,  1, 31, 43, 61,
            18,  2, 32, 44, 62,
            19,  3, 33, 45, 63,
            20,  4, 34, 46,
            21,  5, 35, 47,
            22,  6, 36, 48,
            23,  7, 37, 49,
            24,  8, 38, 50,
            25,  9, 39, 51,
            26, 10, 40, 52,
            27, 11, 41, 53,
            28, 12,     54,
            29, 13,     55,
                14,     56,
                15,     57,
                        58,
                        59,
        };
        for(int unsigned SizeIndex = 0;
            SizeIndex < SIZE_TYPE_COUNT;
            ++SizeIndex)
        {
            int unsigned TestCount = ArrayCount(Tests->Sizes);
            
            int unsigned SizeType = SizePattern[SizeIndex];
            InitializeTests(Tests, SizeType, MaxClockCount);
            fprintf(stdout, "\n----------------------------------------------------\n");
            fprintf(stdout, "\n[%u / %u] %s\n", SizeIndex + 1, SIZE_TYPE_COUNT, Tests->Name);
            fprintf(stdout, "\n----------------------------------------------------\n");
            
            //
            // NOTE(casey): Run the test sizes through each hash, "randomizing" the order to hopefully thwart the branch predictors as much as possible
            //
            
            if(Buffer)
            {
                meow_u64 RunsPerHashImplementation = Tests->RunsPerHashImplementation;
                meow_u64 TestRandSeed = (meow_u64)time(0);
                for(int unsigned TypeIndex = 0;
                    TypeIndex < TypeCount;
                    ++TypeIndex)
                {
                    named_hash_type Type = NamedHashTypes[TypeIndex];
                    fprintf(stdout, "\n%s:\n", Type.FullName);
                    
                    // NOTE(casey): Clear the clock count
                    for(int unsigned SizeIndex = 0;
                        SizeIndex < ArrayCount(Tests->Sizes);
                        ++SizeIndex)
                    {
                        Tests->Sizes[SizeIndex].ClockCount = 0;
                        Tests->Sizes[SizeIndex].ClockAccum = 0;
                        Tests->Sizes[SizeIndex].ClockExp = -1ULL;
                        Tests->Sizes[SizeIndex].ClockMin = -1ULL;
                    }
                    
                    TRY
                    {
                        meow_u64 ClocksSinceLastStatus = 0;
                        meow_u64 TestRand = TestRandSeed;
                        for(int unsigned RunIndex = 0;
                            RunIndex < Tests->RunsPerHashImplementation;
                            ++RunIndex)
                        {
                            int unsigned UseIndex = Random(&TestRand);
                            input_size_test *Test = Tests->Sizes + (UseIndex % TestCount);
                            
                            meow_u64 Size = Test->Size;
                            
                            // NOTE(casey): Write junk into the buffer to try to thwart the optimizer from removing the actual function call.
                            // This should also warm the cache so that small inputs will be read from cache instead of from memory.
                            FuddleBuffer(Size, Buffer, RunIndex);
                            
                            int Ignored[4];
                            int unsigned Ignored2;
                            CPUID(Ignored, 0);
                            meow_u64 StartClock = __rdtsc();
                            Test->FakeSlot = Type.Imp(MeowDefaultSeed, Size, Buffer);
                            meow_u64 EndClock = __rdtscp(&Ignored2);
                            CPUID(Ignored, 0);
                            
                            meow_u64 Clocks = EndClock - StartClock;
                            Test->ClockCount += 1;
                            Test->ClockAccum += Clocks;
                            
                            if(Test->ClockMin > Clocks)
                            {
                                Test->ClockMin = Clocks;
                            }
                            
                            int ClocksPerAvg = Tests->ClocksPerAvg;
                            if(Test->ClockCount == ClocksPerAvg)
                            {
                                meow_u64 ExpClocks = Test->ClockAccum / Test->ClockCount;
                                if(Test->ClockExp > ExpClocks)
                                {
                                    Test->ClockExp = ExpClocks;
                                }
                                
                                Test->ClockAccum = 0;
                                Test->ClockCount = 0;
                            }
                            
                            ClocksSinceLastStatus += Clocks;
                            if((RunIndex == (RunsPerHashImplementation - 1)) ||
                               (ClocksSinceLastStatus > 1000000000ULL))
                            {
                                ClocksSinceLastStatus = 0;
                                fprintf(stdout, "\r    Test %0.0f / %0.0f (%0.0f%%)",
                                        (double)(RunIndex + 1), (double)RunsPerHashImplementation,
                                        100.0f * (double)(RunIndex + 1) / (double)RunsPerHashImplementation);
                                fflush(stdout);
                            }
                        }
                        fprintf(stdout, "\n");
                        
                        for(int TestIndex = 0;
                            TestIndex < TestCount;
                            ++TestIndex)
                        {
                            input_size_test *Test = Tests->Sizes + TestIndex;
                            test_results *Results = CommitResults(TypeIndex, Test, Tests);
                            if(Results)
                            {
                                fprintf(stdout, "    ");
                                PrintSize(stdout, Test->Size, true);
                                
                                fprintf(stdout, ": %0.03f bytes/cycle (%0.0f min, %0.0f exp)\n",
                                        (double)Results->ExpBPC, (double)Results->MinClocks, (double)Results->ExpClocks);
                            }
                            else
                            {
                                fprintf(stderr, "ERROR: Out of result space!\n");
                            }
                        }
                    }
                    CATCH
                    {
                        fprintf(stderr, "    (%s not supported on this CPU)\n",
                                Type.FullName);
                    }
                }
                
                //
                // NOTE(casey): Print the incremental leaderboard
                //
                PrintLeaderboard(Tests, stdout);
                fflush(stdout);
                
                //
                // NOTE(casey): Print any output files we were asked to make
                //
                
                if(CSVFileName)
                {
                    WriteCSV(Tests, CSVFileName);
                }
                
                if(HTMLFileName)
                {
                    WriteHTML(Tests, HTMLFileName);
                }
            }
            
            fprintf(stdout, "\n");
            fflush(stdout);
            fflush(stderr);
        }
        
        free(Buffer);
    }
    else
    {
        fprintf(stderr, "ERROR: Unable to allocate buffer for hashing\n");
    }
    
#if __aarch64__
    disable_pmu(0x008);
#endif
    
    return(0);
}
