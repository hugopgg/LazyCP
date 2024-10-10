all: lcp

lcp: lcp.c checksum.o
	$(CC) -Wextra -Wall -o lcp lcp.c checksum.o

checksum.o:
	$(CC) -c checksum.c

clean:
	rm lcp checksum.o 
