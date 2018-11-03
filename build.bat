@echo off

where cl >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (echo WARNING: cl is not in the path - please set up Visual Studio to do cl builds) && goto SkipMSVC

echo -------------------
echo Building with MSVC

if not exist "build_msvc" mkdir build_msvc
pushd build_msvc
cl %* -I../ -nologo -FC -Oi -O2 -Zi ..\meow_example.cpp
cl %* -I../ -nologo -EHsc -FC -Oi -O2 -Zi ..\utils\meow_test.cpp
cl %* -I../ -nologo -FC -Oi /O2 -Zi -arch:AVX ..\utils\meow_search.cpp
cl %* -I../ -nologo -FC -Oi /O2 -Zi -arch:AVX2 ..\utils\meow_bench.cpp
popd

:SkipMSVC


where clang++ >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (echo WARNING: clang++ is not in the path - please set up LLVM to do clang++ builds) && goto SkipCLANG

echo -------------------
echo Building with CLANG

if not exist "build_clang" mkdir build_clang
pushd build_clang
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -msse4 ..\meow_example.cpp -o meow_example.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -msse4 ..\utils\meow_test.cpp -o meow_test.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -mavx ..\utils\meow_search.cpp -o meow_search.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -mavx2 ..\utils\meow_bench.cpp -o meow_bench.exe
popd

echo -------------------

:SkipCLANG
