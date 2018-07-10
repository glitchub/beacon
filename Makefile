.PHONY: default clean
CFLAGS+= -Wall -Werror -std=gnu99
default: beacon
clean:; rm beacon
