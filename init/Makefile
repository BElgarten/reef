.POSIX:

OBJS = init.o
BIN = init

LINKERSCRIPT = init.ld

AS = clang
LD = x86_64-pe-ld 
CC = clang

CFLAGS = -I$(INCLUDEDIR) -target x86_64-w64-windows-gnu -ffreestanding -fshort-wchar -mno-red-zone -Werror -std=c99 -pedantic -pedantic-errors
ASFLAGS = -target x86_64-w64-windows-gnu -Wall -Wextra -Werror
LDFLAGS = -T$(LINKERSCRIPT)

.PHONY: build clean
.SUFFIXES: .c .s .S .o

build: $(BIN)

clean:
	rm -f $(OBJS)
	rm -f $(BIN)

$(BIN): $(OBJS) $(LINKERSCRIPT)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<
