// Duvidas para tirar com edmar:
// Como criar porta randomica funcional?

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

#define porta_servidor 7325
#define h_addr h_addr_list[0]

int main(int argc, char *argv[])
{
    int operacao;

    char buffer_envio[50];
    char buffer_recebimento[50];

    struct hostent *hostnm;
    struct sockaddr_in server;

    int socket_recebe, socket_envia;
    int ip_recebimento, porta_recebimento;
    char telefone[50];

    printf("Informe seu telefone: ");
    fgets(telefone, 50, stdin);

    if (argc == 3)
    {
        hostnm = gethostbyname(argv[1]);
        if (hostnm == (struct hostent *)0)
        {
            fprintf(stderr, "Gethostbyname failed\n");
            exit(2);
        }

        server.sin_family = AF_INET;
        server.sin_port = htons(porta_servidor);
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);
    }
    else
    {
        hostnm = gethostbyname("localhost");
        if (hostnm == (struct hostent *)0)
        {
            fprintf(stderr, "Gethostbyname failed\n");
            exit(2);
        }

        server.sin_family = AF_INET;
        server.sin_port = htons(porta_servidor);
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);
    }

    socket_envia = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_envia < 0)
    {
        perror("Socket()");
        exit(3);
    }

    connect(socket_envia, (struct sockaddr *)&server, sizeof(server));
    if (socket_envia < 0)
    {
        perror("Connect()");
        exit(4);
    }

    //sprintf(buffer_envio, "%i", telefone);
    if (send(socket_envia, telefone, sizeof(telefone), 0) < 0)
    {
        perror("ERRO - send(ctS)");
    }

    srand(time(NULL));
    porta_recebimento = rand()%65500;
    sprintf(buffer_envio, "%i", porta_recebimento);
    if (send(socket_envia, buffer_envio, sizeof(buffer_envio), 0) < 0)
    {
        perror("ERRO - send(ctS)");
    }


    do
    {
        printf("Escolha uma das opcoes a seguir: \n");
        printf("1 para Enviar Mensagem. \n");
        printf("2 para Receber Mensagem. \n");
        printf("3 para Criar Grupo. \n");
        printf("4 para Adicionar Contato. \n");
        printf("5 para Vizualizar os Creditos. \n");
        printf("0 para sair. \n");
        scanf("%i", &operacao);
        //system("clear");

        switch (operacao)
        {
        case 1: //Enviar Msg
        {
        }
        break;

        case 2: //Receber Msg
        {
        }
        break;

        case 3: //Criar Grupo
        {
        }
        break;

        case 4: //Adc Contato
        {
        }
        break;

        case 5: //Creditos
        {
        }
        break;

        default:
            break;
        }

    } while (operacao != 0);

    close(socket_envia);
}