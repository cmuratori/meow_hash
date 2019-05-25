@echo off

setlocal

where cl >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (echo WARNING: cl is not in the path - please set up Visual Studio to do cl builds) && goto SkipMSVC

echo -------------------------------------
echo Building with MSVC

if not exist "build_msvc" mkdir build_msvc
pushd build_msvc
set common=-I../ -nologo -FC -Oi -O2 -Zi %*
call cl %common% -EHsc -TC ..\meow_example.cpp -Femeow_example_c.exe
call cl %common% -EHsc ..\meow_example.cpp -Femeow_example.exe
call cl %common% -EHsc ..\util\meow_test.cpp -Femeow_test.exe
call cl %common% -arch:AVX2 ..\util\meow_search.cpp -Femeow_search.exe
call cl %common% -arch:AVX2 ..\util\meow_bench.cpp -Femeow_bench.exe
popd

:SkipMSVC

where clang++ >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (echo WARNING: clang++ is not in the path - please set up LLVM to do clang++ builds) && goto SkipCLANG

echo -------------------------------------
echo Building with CLANG

if not exist "build_clang" mkdir build_clang
pushd build_clang
set common=-I../ -Wno-deprecated-declarations -g -O3 -maes %*
call clang++ %common% -msse4 ..\meow_example.cpp -o meow_example.exe
call clang++ %common% -msse4 ..\util\meow_test.cpp -o meow_test.exe
call clang++ %common% -mavx2 -mpclmul ..\util\meow_search.cpp -o meow_search.exe
call clang++ %common% -mavx2 -mpclmul ..\util\meow_bench.cpp -o meow_bench.exe
popd

echo -------------------

:SkipCLANG
