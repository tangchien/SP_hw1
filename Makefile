#	This file for testing
all : write_server read_server

write_server : server.c
	gcc server.c -D WRITE_SERVER -o write_server
read_server : server.c
	gcc server.c -D READ_SERVER -o read_server
	
