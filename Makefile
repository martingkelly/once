CC=gcc
CFLAGS=-O2 --std=c99 -Wall -Werror -Wextra
SOURCES=solo.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=solo

$(EXECUTABLE): $(OBJECTS)
	$(CC) $< -o $@

clean:
	@rm -f $(OBJECTS) $(EXECUTABLE)
