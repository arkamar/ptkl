CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic -Werror=implicit-function-declaration
CFLAGS += -fPIC
LDLIBS += -ldl

NAME = ptkl

all: lib${NAME}.so

%.so: %.o
	$(CC) -shared $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	$(RM) *.o *.so
