#!/bin/bash
model=pic
#./bin/clang ~/main.c -c -emit-llvm -O$1 --target=sodium16 -o ~/main.bc
#./bin/llvm-dis ~/main.bc
#cat ~/main.ll
#./bin/llc -relocation-model=$model -filetype=asm -march=sodium16 ~/main.bc -o main.s
#./bin/llc -relocation-model=$model -filetype=obj -march=sodium16 ~/main.bc -o main.o -debug
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/main.c
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/crt.S
./bin/ld.lld crt.o main.o
./bin/llvm-objdump -d a.out &> ass.S
./bin/llvm-objcopy -O binary a.out test_instructions.bin
