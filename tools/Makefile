.POSIX:

BINS = gpt_creator

CC = cc
CFLAGS = -Wall -Wextra -pedantic -ansi -Werror

SRCDIR = src

.PHONY: build clean
build: $(BINS)

clean:
	rm -f $(BINS)

%:$(SRCDIR)/%.c
	$(CC) $(CFLAGS) $< -o $@
