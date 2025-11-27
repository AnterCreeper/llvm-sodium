#!/bin/bash
model=pic
./bin/clang --target=sodium16 -fPIC -O$1 -c ~/sodium/coremark/*.c
./bin/clang --target=sodium16 -fPIC -c ~/crt.S
./bin/ld.lld -T ../link.ld crt.o core_list_join.o core_util.o core_main.o core_matrix.o core_portme.o core_state.o libc.o divsi.o divhi.o ee_printf.o
./bin/llvm-objdump -d a.out &> result.S
./bin/llvm-objcopy a.out -O binary test_instructions.bin
