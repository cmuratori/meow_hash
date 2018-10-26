@echo off
mkdir build
pushd build
cl %* -I../ /FC /Oi /O2 /Zi /arch:AVX ..\meow_example.cpp
cl %* -I../ /EHsc /FC /Oi /O2 /Zi /arch:AVX ..\utils\meow_test.cpp
cl %* -I../ /FC /Oi /O2 /Zi /arch:AVX ..\utils\meow_search.cpp
cl %* -I../ /FC /Oi /O2 /Zi /arch:AVX ..\utils\meow_bench.cpp
popd