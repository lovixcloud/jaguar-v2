CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -D_GNU_SOURCE -g -O2 -I. -Iruntime
LDFLAGS ?= -lpthread -lssl -lcrypto -lm

RUNTIME_SRCS = $(wildcard runtime/core/*.c runtime/io/*.c runtime/json/*.c runtime/async/*.c runtime/http/*.c runtime/ws/*.c runtime/worker/*.c)
COMPILER_SRCS = $(wildcard lexer/*.c parser/*.c ast/*.c typecheck/*.c backend_vm/*.c backend_c/*.c)
CLI_SRCS = src/cli.c

RUNTIME_OBJS = $(RUNTIME_SRCS:.c=.o)
COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
CLI_OBJS = $(CLI_SRCS:.c=.o)

LIBJAGRT = libjagrt.a

all: $(LIBJAGRT) jag

$(LIBJAGRT): $(RUNTIME_OBJS)
	@if [ -n "$(RUNTIME_OBJS)" ]; then \
		ar rcs $@ $(RUNTIME_OBJS); \
	else \
		touch $@; \
	fi

jag: $(COMPILER_OBJS) $(CLI_OBJS) $(LIBJAGRT)
	$(CC) $(CFLAGS) -o $@ $(COMPILER_OBJS) $(CLI_OBJS) $(LIBJAGRT) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(RUNTIME_OBJS) $(COMPILER_OBJS) $(CLI_OBJS) $(LIBJAGRT) jag tests/test_*

.PHONY: all clean
