CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = quash
SOURCES = shell.c

$(TARGET): $(SOURCES)
      $(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
      rm -f $(TARGET) test_output test_input

.PHONY: clean