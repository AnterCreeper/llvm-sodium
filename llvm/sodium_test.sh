#!/bin/bash
model=pic
./bin/clang ~/main.cpp -c -emit-llvm -O$1 --target=sodium16 -o ~/main.bc
./bin/llvm-dis ~/main.bc
cat ~/main.ll
./bin/llc -relocation-model=$model -filetype=asm -march=sodium16 ~/main.bc -o main.s
./bin/llc -relocation-model=$model -filetype=obj -march=sodium16 ~/main.bc -o main.o -debug
./bin/llvm-objdump -d main.o
./bin/llvm-objdump -tr main.o
