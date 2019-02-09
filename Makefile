CC = gcc
DIRECTORIES = $(wildcard */)

make: server.o client.o
	$(CC) -o client client.o
	$(CC) -o server server.o

.PHONY: clean

clean:
	@rm client
	@rm server
	@if [ $(DIRECTORIES) ]; then rm -r $(DIRECTORIES); fi;
