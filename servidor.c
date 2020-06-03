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
#include <time.h>
#include <pthread.h>

#define porta_servidor 7325

struct cliente
{
    int telefone;
    int porta;
    struct sockaddr_in cliente_endereco;
    struct cliente *prox;

} typedef cliente;

struct
{
    int socket_redirecionado;
    struct sockaddr_in cliente_endereco;

} typedef thread_args, *ptr_thread_arg;

cliente *lista_clientes;

void *thread_cliente(void *argumentos);
int registrar_cliente(int telefone, int porta, struct sockaddr_in endereco);
int excluir_cliente(int telefone);
cliente procurar_cliente(int telefone);

int main(int argc, char *argv[])
{
    int comprimento;

    int socket_conexao;
    int socket_redirecionado;

    struct sockaddr_in cliente_endereco;

    struct sockaddr_in cliente;
    struct sockaddr_in servidor;

    pthread_t thread_id;
    thread_args argumentos;

    if ((socket_conexao = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket()");
        exit(2);
    }

    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(porta_servidor);
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

    do
    {
        comprimento = sizeof(cliente);
        if ((socket_redirecionado = accept(socket_conexao, (struct sockaddr *)&cliente, (socklen_t *)&comprimento)) == -1)
        {
            perror("Accept()");
            exit(5);
        }

        cliente_endereco.sin_addr = cliente.sin_addr;

        argumentos.cliente_endereco = cliente_endereco;
        argumentos.socket_redirecionado = socket_redirecionado;
        pthread_create(&thread_id, NULL, &thread_cliente, &argumentos);

    } while (1);
    close(socket_conexao);
    return EXIT_SUCCESS;
}

void *thread_cliente(void *arg)
{
    ptr_thread_arg argumentos = (ptr_thread_arg)arg;
    int socket_redirecionado = argumentos->socket_redirecionado;
    struct sockaddr_in cliente_endereco = argumentos->cliente_endereco;

    char buffer_envia[50];
    char buffer_recebe[50];

    int telefone_cliente, porta_envio_cliente;

    if (recv(socket_redirecionado, buffer_recebe, sizeof(buffer_recebe), 0) == -1)
    {
        fprintf(stderr, "ERRO - Recv(ctS): %s\n", strerror(errno));
        exit(-1);
    }
    telefone_cliente = atoi(buffer_recebe);

    if (recv(socket_redirecionado, buffer_recebe, sizeof(buffer_recebe), 0) == -1)
    {
        fprintf(stderr, "ERRO - Recv(ctS): %s\n", strerror(errno));
        exit(-1);
    }
    porta_envio_cliente = atoi(buffer_recebe);

    printf("Telefone: %d \n", telefone_cliente);
    printf("Porta: %d\n", porta_envio_cliente);
    printf("Endereco: %s \n", inet_ntoa(cliente_endereco.sin_addr));

    //registrar_cliente(telefone_cliente, porta_envio_cliente, cliente_endereco);

    close(socket_redirecionado);

    pthread_exit(NULL);
}

int registrar_cliente(int telefone, int porta, struct sockaddr_in endereco)
{
    cliente *pont_auxiliar = lista_clientes;

    if (lista_clientes == NULL)
    {
        lista_clientes = (cliente *)malloc(sizeof(cliente));
        if (lista_clientes == NULL)
        {
            perror("Malloc");
            return 1; //Codigo para erro
        }

        lista_clientes->cliente_endereco = endereco;
        lista_clientes->porta = porta;
        lista_clientes->telefone = telefone;
    }
    else
    {
        while (pont_auxiliar->prox == NULL)
        {
            pont_auxiliar = pont_auxiliar->prox;
        }

        pont_auxiliar = (cliente *)malloc(sizeof(cliente));
        if (pont_auxiliar == NULL)
        {
            perror("Malloc");
            return 1; //Codigo para erro
        }

        pont_auxiliar->cliente_endereco = endereco;
        pont_auxiliar->porta = porta;
        pont_auxiliar->telefone = telefone;
    }

    free(pont_auxiliar);
    return 0; //0 para OK, 1 para Erro
}

int excluir_cliente(int telefone)
{

    return 0; //0 para OK, 1 para Erro
}

cliente procurar_cliente(int telefone)
{
}