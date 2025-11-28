#!/bin/bash
model=pic
./bin/clang --target=sodium16 -fPIC -c ~/crt.S
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/main.c
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/ee_printf.c
./bin/ld.lld -T ../link.ld crt.o main.o ee_printf.o
./bin/llvm-objdump -d a.out &> result.S
./bin/llvm-objcopy a.out -O binary test_instructions.bin
