#!/bin/sh

set -eu

CXX=${CXX:-clang++}

mkdir -p build
${CXX} $* -I. meow_example.cpp -O3 -mavx -maes -o build/meow_example
${CXX} $* -I. utils/meow_test.cpp -O3 -mavx -maes -o build/meow_test
${CXX} $* -I. utils/meow_search.cpp -O3 -mavx -maes -o build/meow_search
${CXX} $* -I. utils/meow_bench.cpp -O3 -mavx -maes -o build/meow_bench
