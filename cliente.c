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

#define porta_servidor  7325

int main(int argc, char *argv[])
{
    time_t seed;
    int operacao;

    char buffer_envio[50];              
    char buffer_recebimento[50]; 

    struct hostent *hostnm;    
    struct sockaddr_in server; 

    int socket_recebe, socket_envia;
    int ip_recebimento, porta_recebimento, telefone;
    
    printf("Informe seu telefone: ");
    scanf("%i", &telefone);
    
    if(argc == 3)
    {
        hostnm = gethostbyname(argv[1]);
        if (hostnm == (struct hostent *) 0)
        {
            fprintf(stderr, "Gethostbyname failed\n");
            exit(2);
        }

        server.sin_family      = AF_INET;
        server.sin_port        = htons(porta_servidor);
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);
    }
    else
    {
        hostnm = gethostbyname("localhost");
        if (hostnm == (struct hostent *) 0)
        {
            fprintf(stderr, "Gethostbyname failed\n");
            exit(2);
        }

        server.sin_family      = AF_INET;
        server.sin_port        = htons(porta_servidor);
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
   
    sprintf(buffer_envio,"%d",telefone);
    send(socket_envia, buffer_envio, strlen(buffer_envio)+1, 0);
    if (socket_envia < 0)
    {
        perror("Send()");
        exit(5);
    }

    srand((unsigned) time(&seed));   ///////////////////????????????????????????????????
    porta_recebimento = rand()%65500;

    sprintf(buffer_envio,"%d",porta_recebimento);
    send(socket_envia, buffer_envio, strlen(buffer_envio)+1, 0);
    if (socket_envia < 0)
    {
        perror("Send()");
        exit(5);
    }

    do
    {
        printf("Digite a operacao desejada ou 1 para ajuda: ");
        scanf("%i",  &operacao);

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

        default:
            break;
        }
        
    } while (operacao != 0);
    
    
    close(socket_envia);
}