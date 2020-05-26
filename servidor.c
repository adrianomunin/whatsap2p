#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#define porta_servidor  7325

void thread_cliente(int socket_redirecionado);

int main(int argc, char *argv[])
{
    int comprimento;

    int socket_conexao;
    int socket_redirecionado;

    struct sockaddr_in cliente; 
    struct sockaddr_in servidor; 


    if ((socket_conexao = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket()");
        exit(2);
    }

    servidor.sin_family = AF_INET;   
    servidor.sin_port   = htons(porta_servidor);      
    servidor.sin_addr.s_addr = INADDR_ANY;

    bind(socket_conexao, (struct sockaddr *)&servidor, sizeof(servidor));
    if (socket_conexao < 0)
    {
       perror("Bind()");
       exit(3);
    }

    if (listen(socket_conexao, 1) != 0)
    {
        perror("Listen()");
        exit(4);
    }

    comprimento = sizeof(cliente);
    if ((socket_redirecionado = accept(socket_conexao, (struct sockaddr *)&cliente, (socklen_t *)&comprimento)) == -1)
    {
        perror("Accept()");
        exit(5);
    }

    thread_cliente(socket_redirecionado);

    close(socket_conexao);
}

void thread_cliente(int socket_redirecionado)
{
    char buffer_envia[12];              
    char buffer_recebe[12]; 

    if (recv(socket_redirecionado, buffer_recebe, sizeof(buffer_recebe), 0) == -1)
    {
        perror("Recv()");
        exit(6);
    }
    printf("Mensagem recebida do cliente: %s\n", buffer_recebe);

    if (recv(socket_redirecionado, buffer_recebe, sizeof(buffer_recebe), 0) == -1)
    {
        perror("Recv()");
        exit(6);
    }
    printf("Mensagem recebida do cliente: %s\n", buffer_recebe);

    close(socket_redirecionado);
}