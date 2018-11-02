mkdir -p build
clang++ $* -I./ meow_example.cpp -O3 -mavx -maes -obuild/meow_example
clang++ $* -I./ utils/meow_test.cpp -O3 -mavx -maes -obuild/meow_test
clang++ $* -I./ utils/meow_search.cpp -O3 -mavx -maes -obuild/meow_search
clang++ $* -I./ utils/meow_bench.cpp -O3 -mavx -maes -obuild/meow_bench
