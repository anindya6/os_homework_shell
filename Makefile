CC := gcc
CFLAGS := -Wall -Werror

all: w4118_sh

w4118_sh:
	$(CC) $(CFLAGS) -o $@ shell.c

clean:
	rm -f w4118_sh

.PHONY: clean
