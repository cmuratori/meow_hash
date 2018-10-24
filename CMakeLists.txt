project (meow_hash)
cmake_minimum_required (VERSION 3.2)

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -std=c++11")
endif ()

# NOTE(qix-): __try is a massive overhead right now. it's not standard C++, but
#             instead an MSVC extension :/ in reality it should be refactored
#             to use the SIMD predef's: https://stackoverflow.com/a/28939692
#             with special attention to:
#             https://stackoverflow.com/questions/28939652/how-to-detect-sse-sse2-avx-avx2-avx-512-avx-128-fma-kcvi-availability-at-compile#comment60183290_28939692
#             as such, the following cases have been removed from non-win32 builds.
if (WIN32)
    add_executable (meow_smhasher meow_smhasher.cpp)
    add_executable (meow_bench meow_bench.cpp)
    add_executable (meow_example meow_example.cpp)
endif ()
