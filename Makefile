CC ?= clang
OPTFLAGS ?= -O3 -march=native
DBGFLAGS ?= -DEBUG -g
CFLAGS ?= -Wall -Wextra -pedantic -std=gnu99

ifeq ($(shell uname -s),Darwin)
	LDLIBS ?= -framework OpenCL
else
	LDLIBS ?= -lOpenCL
endif

.PHONY: all
all:
	$(CC) $(CFLAGS) $(OPTFLAGS) -o brutenonce brutenonce.c $(LDLIBS)

.PHONY: debug
debug:
	$(CC) $(CFLAGS) $(DBGFLAGS) -o brutenonce brutenonce.c $(LDLIBS)

.PHONY: clean
clean:
	$(RM) brutenonce
