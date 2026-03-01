#
# keonOS - user/Makefile
# Copyright (C) 2025-2026 fmdxp
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
# - Original author attributions must be preserved in all copies.
# - Modified versions must be marked as different from the original.
# - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#

CC = ../cross/bin/x86_64-elf-gcc
LD = ../cross/bin/x86_64-elf-ld
AR = ../cross/bin/x86_64-elf-ar
ASM = nasm

CFLAGS = -ffreestanding -O2 -nostdlib -fno-rtti -fno-exceptions -z max-page-size=0x1000 -mcmodel=large -Ilibc/include -Ilibkex/include
ASMFLAGS = -f elf64

LIBC_C_SRC = $(wildcard libc/*/*.c) $(wildcard libc/*/*/*.c)
LIBC_ASM_SRC = libc/crt0.asm libc/syscall.asm

LIBKEX_SRC = libkex/kex_file.c
LIBKEX_OBJ = $(patsubst %.c,%.o,$(LIBKEX_SRC))

LIBC_OBJ = $(patsubst %.c,%.o,$(LIBC_C_SRC)) $(patsubst %.asm,%.o,$(LIBC_ASM_SRC))

all: apps tools tests libs

libc.klb: $(LIBC_OBJ)
	$(AR) rcs $@ $^

libkex.klb: $(LIBKEX_OBJ)
	$(AR) rcs $@ $^

libs: math.kdl

math.kdl: libkex/libmath.c
	$(CC) $(CFLAGS) -fPIC -c libkex/libmath.c -o libkex/libmath.o
	$(LD) -m elf_x86_64 -shared -o $@ libkex/libmath.o

apps: hello.kex

tools: klbtool.kex

tests: test_file.kex test_sys.kex test_kdl.kex

hello.kex: hello.o libc.klb libkex.klb
	$(LD) -T kex.ld -o $@ libc/crt0.o hello.o libc.klb libkex.klb

klbtool.kex: tools/klbtool.o libc.klb libkex.klb
	$(LD) -T kex.ld -o $@ libc/crt0.o tools/klbtool.o libc.klb libkex.klb

test_file.kex: tests/test_file.o libc.klb
	$(LD) -T kex.ld -o $@ libc/crt0.o tests/test_file.o libc.klb

test_sys.kex: tests/test_sys.o libc.klb
	$(LD) -T kex.ld -o $@ libc/crt0.o tests/test_sys.o libc.klb

test_kdl.kex: tests/test_kdl.o libc.klb
	$(LD) -T kex.ld -o $@ libc/crt0.o tests/test_kdl.o libc.klb

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c
	@mkdir -p tests
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

clean:
	rm -f $(LIBC_OBJ) libc.klb hello.o hello.kex *.o *.kdl
