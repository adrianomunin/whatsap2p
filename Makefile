all: 
	gcc cliente.c -o cliente -g -pthread 
	gcc servidor.c -o servidor -g -pthread

cliente: cliente.c
	gcc cliente.c -o cliente

servidor: servidor.c
	gcc servidor.c -o servidor

