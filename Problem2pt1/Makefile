CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = problem2pt1
SRC = problem2pt1.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

output: $(TARGET)
	./$(TARGET) > output.txt

clean:
	rm -f $(TARGET) output.txt

