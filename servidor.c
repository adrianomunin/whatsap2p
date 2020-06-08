#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h> 

#define DEBUG 1

//comandos
#define ENCERRAR "encerrar" // encerra conexao, o que torna offline
#define GETLOC "getloc" // requere informacao do servidor, necessario informar telefone
#define GETTEL "gettel" // requere informacao do servidor, necessario informar telefone
#define NOTFOUND "notfound" //informacao nao encontrada

typedef struct no{
    char telefone[20];
    struct sockaddr_in localizacao;
    struct no *prox;
    struct no *ant;
}usuario;

//mutex
pthread_mutex_t mutex;

//thread_args
typedef struct{
    int socket;
    struct sockaddr_in myName;

}thread_arg,*ptr_thread_arg;

//thread_func
void *thread_cliente(void*);

//lista de usuarios
usuario *listaDeUsuarios;

/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_usuario(char *tel);
/*Adiciona o usuario no fim da lista, caso usuario ja exista chama ´put_online´*/
void adiciona_usuario(usuario add);
/*Define o usuario com ´tel´ como offline*/
int put_offline(char *tel);
/*Define o usuario com ´tel´ como online*/
int put_online(char *tel,struct sockaddr_in);


/*printa a lista*/
void print_lista(){
    usuario *aux = listaDeUsuarios;
    while(aux!=NULL){
        printf("Telefone: %s\n",aux->telefone);
        aux = aux->prox;
    }
}

struct sockaddr_in getLoc(char *telefone);
usuario getTel(struct sockaddr_in);



int main(int argc, char *argv[])
{
    int namelen;
    int socket_conexao;
    int socket_thread;

    struct sockaddr_in cliente; 
    struct sockaddr_in servidor; 

    //variaveis thread
    int thread_create_result;
    pthread_t ptid;
    thread_arg t_arg;
    
    //init_mutex
    if(pthread_mutex_init(&mutex,NULL) != 0){
        perror("ERRO - mutex_init()");
        exit(errno);
    }

/*Configuração do servidor*/

    //Socket init
    if ((socket_conexao = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket()");
        exit(errno);
    }

    //Digitou errado, companheiro!
    if(argc != 2){
        printf("Use %s <porta>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    servidor.sin_family = AF_INET;   
    servidor.sin_port   = htons(atoi(argv[1]));      
    servidor.sin_addr.s_addr = INADDR_ANY;

    if(ntohs(servidor.sin_port) == 0){
        printf("ERRO - Porta invalida\n");
        exit(EXIT_FAILURE);
    }

    if (bind(socket_conexao, (struct sockaddr *)&servidor, sizeof(servidor)) != 0)
    {
       perror("ERRO - Bind()");
       exit(errno);
    }
    
    if (listen(socket_conexao, 1) != 0)
    {
        perror("ERRO - Listen()");
        exit(errno);
    }

    system("clear");
    printf("Servidor WhatsAp2p iniciado na porta %i\n\n",ntohs(servidor.sin_port));
    
        do{        
        namelen = sizeof(cliente);
        if ((socket_thread = accept(socket_conexao, (struct sockaddr *)&cliente, (socklen_t *)&namelen)) == -1)
        {
            perror("ERRO - Accept()");
            exit(errno);
        }

        t_arg.socket = socket_thread;
        t_arg.myName = cliente;

        thread_create_result = pthread_create(&ptid,NULL,&thread_cliente,&t_arg);
        if(thread_create_result != 0){
            perror("ERRO - thread_create()");
            exit(thread_create_result);
        }

    }while(1);
    close(socket_conexao);
    pthread_mutex_destroy(&mutex);
    printf("Servidor encerrado\n");
    return EXIT_SUCCESS;
}

void *thread_cliente(void *arg)
{
    char buffer_envia[80];              
    char buffer_recebe[80];
    char *msg[80];
    char aux[80];
    usuario cliente;

    ptr_thread_arg thread_arg = (ptr_thread_arg)arg;

    int socket = thread_arg->socket;
    struct sockaddr_in whoami = thread_arg->myName;
    struct sockaddr_in query;
    usuario queryU;

    //Recebimento da porta de escuta e telefone
    if(recv(socket,buffer_recebe,sizeof(buffer_recebe),0) == -1){
        perror("ERRO - Recv(porta,telefone");
        exit(errno);
    }
   
    msg[0]=strtok(buffer_recebe,";\n");
    msg[1]=strtok(NULL,";\n");

    
    whoami.sin_port = htons(atoi(msg[1]));
    cliente.localizacao = whoami;
    strcpy(cliente.telefone,msg[0]);


    //Atualizacao da lista de clientes
    pthread_mutex_trylock(&mutex);
        adiciona_usuario(cliente);
    pthread_mutex_unlock(&mutex);

    printf("cliente id: %s CONECTADO\n",cliente.telefone);   
    do{
        *msg=NULL;
    
        if(recv(socket,buffer_recebe,sizeof(buffer_recebe),0) == -1){
            fprintf(stderr,"ERRO - Recv(): %s, cliente id: %s\n",strerror(errno),cliente.telefone);
            break;
        }
        msg[0] = strtok(buffer_recebe,";\n");//comando
        msg[1] = strtok(NULL,";\n");//parametros (ip/hostname,telefone)
        msg[2] = strtok(NULL,";\n");//parametros ip/hostname (porta) ,se msg[1]=telefone msg[2]=NULL
        
        #ifdef DEBUG
        printf("Comando recebido= %s - %s - %s\n\n",msg[0],msg[1],msg[2]);
        #endif

        if(msg[0] == NULL) break;
        if(strcmp(msg[0],GETLOC) == 0){
            //Requisicao de informações de localizacao
            printf("Requisicao GETLOC de cliente id: %s\n",cliente.telefone);
            pthread_mutex_trylock(&mutex);
            query = getLoc(msg[1]);
            pthread_mutex_unlock(&mutex);
            if(query.sin_addr.s_addr == INADDR_NONE){
                if(send(socket,NOTFOUND,sizeof(NOTFOUND),0)<0){
                    fprintf(stderr,"ERRO - Send(): %s, cliente id: %s\n",strerror(errno),cliente.telefone);
                    break;
                }
                #ifdef DEBUG
                printf("Comando enviado= %s\n\n",NOTFOUND);
                #endif
            }else{
                strcpy(buffer_envia,inet_ntoa(query.sin_addr));
                strcat(buffer_envia,";");
                sprintf(aux,"%d",ntohs(query.sin_port));
                strcat(buffer_envia,aux);
                #ifdef DEBUG
                printf("Comando enviado= %s\n\n",buffer_envia);
                #endif
                if(send(socket,buffer_envia,sizeof(buffer_envia),0)<0){
                   fprintf(stderr,"ERRO - Send(): %s, cliente id: %s\n",strerror(errno),cliente.telefone);
                   break;
                }
            }
        }

    }while(strcmp(msg[0],ENCERRAR) != 0);

    close(socket);
    pthread_mutex_trylock(&mutex);
    remove_usuario(cliente.telefone);
    pthread_mutex_unlock(&mutex);
    printf("cliente id: %s DESCONECTADO\n",cliente.telefone);    
}


struct sockaddr_in getLoc(char *telefone){
    
    struct sockaddr_in ret;
    ret.sin_addr.s_addr= INADDR_NONE;
    usuario *lista = listaDeUsuarios;

    while(lista != NULL){
        if(strcmp(lista->telefone,telefone) == 0){
            return lista->localizacao;
        }    
        lista=lista->prox;
    }
    return ret;
}

int remove_usuario(char *tel){

    usuario *aux = listaDeUsuarios;

    while(aux != NULL){
        if(strcmp(aux->telefone,tel) == 0){
            if(aux->ant == NULL){
                listaDeUsuarios = aux->prox;
                free(aux);            
            }else{
                aux->ant->prox = aux->prox;
                free(aux);
            }
            return 1;
        }else{
            aux = aux->prox;
        }
    }
    return 0;
}




/*Adiciona o usuario no fim da lista, caso usuario ja exista chama ´put_online´*/
void adiciona_usuario(usuario add){
    usuario *novo = (usuario *)malloc(sizeof(usuario));
    if(novo == NULL){
        perror("ERRO - malloc(novo)");
        exit(errno);
    }
    strcpy(novo->telefone,add.telefone);
    novo->localizacao = add.localizacao;
    novo->prox=NULL;
        
    if(listaDeUsuarios == NULL){
        listaDeUsuarios = novo;
        listaDeUsuarios->ant = NULL;
    }else{
        usuario *aux = listaDeUsuarios;
        while(aux->prox != NULL){
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant=aux;
    }
}