#!/bin/bash
target=riscv32
#target=arm
clang ~/main.c -c -emit-llvm -O$1 --target=$target -o ~/main.bc
llvm-dis ~/main.bc
cat ~/main.ll
llc -relocation-model=pic -filetype=asm -march=$target ~/main.bc -o main.s
llc -relocation-model=pic -filetype=obj -march=$target ~/main.bc -o main.o
llvm-objdump -d main.o
llvm-objdump -tr main.o
