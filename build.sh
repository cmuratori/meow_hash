#!/bin/sh

set -eu

CXX=${CXX:-clang++}

mkdir -p build
${CXX} $* -I. meow_example.cpp -O3 -mavx -maes -o build/meow_example
${CXX} $* -I. more/meow_more_example.cpp -O3 -mavx -maes -o build/meow_more_example
${CXX} $* -I. more/meow_test.cpp -O3 -mavx -maes -o build/meow_test
${CXX} $* -I. more/meow_search.cpp -O3 -mavx -maes -o build/meow_search
${CXX} $* -I. more/meow_bench.cpp -O3 -mavx2 -maes -o build/meow_bench
