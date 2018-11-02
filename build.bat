@echo off

echo -------------------
echo Building with MSVC

mkdir build_msvc
pushd build_msvc
cl %* -I../ -nologo -FC -Oi -O2 -Zi ..\meow_example.cpp
cl %* -I../ -nologo -EHsc -FC -Oi -O2 -Zi ..\utils\meow_test.cpp
cl %* -I../ -nologo -FC -Oi /O2 -Zi -arch:AVX ..\utils\meow_search.cpp
cl %* -I../ -nologo -FC -Oi /O2 -Zi -arch:AVX2 ..\utils\meow_bench.cpp
popd

echo -------------------
echo Building with CLANG

mkdir build_clang
pushd build_clang
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -msse4 ..\meow_example.cpp -o meow_example.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -msse4 ..\utils\meow_test.cpp -o meow_test.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -mavx ..\utils\meow_search.cpp -o meow_search.exe
clang++ %* -I../ -Wno-deprecated-declarations -g -O3 -maes -mavx2 ..\utils\meow_bench.cpp -o meow_bench.exe
popd

echo -------------------
