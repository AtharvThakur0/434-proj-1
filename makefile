CC = gcc

release:
	$(CC) main.c -o main
debug:
	$(CC) main.c -o main -ggdb -Og
clean:
	rm main
	rm log.txt
	rm trace.csv