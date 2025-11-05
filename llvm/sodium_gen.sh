#!/bin/bash
model=pic
#./bin/clang ~/main.c -c -emit-llvm -O$1 --target=sodium16 -o ~/main.bc
#./bin/llvm-dis ~/main.bc
#cat ~/main.ll
#./bin/llc -relocation-model=$model -filetype=asm -march=sodium16 ~/main.bc -o main.s
#./bin/llc -relocation-model=$model -filetype=obj -march=sodium16 ~/main.bc -o main.o -debug
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/main.c
./bin/clang --target=sodium16 -fPIC -O$1 -S ~/main.c
./bin/llvm-objdump -d main.o
./bin/llvm-objdump -tr main.o
