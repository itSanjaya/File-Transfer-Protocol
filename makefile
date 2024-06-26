all: c s

c: ftp_client.c
	gcc -o c ftp_client.c

s: ftp_server.c
	gcc -o s ftp_server.c