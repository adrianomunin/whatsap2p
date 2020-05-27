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

#define DEBUG 1

//comandos
#define ENCERRAR "encerrar" // encerra conexao, o que torna offline
#define GET "get" // requere informacao do servidor, necessario informar telefone
#define REMOVE "remove" //pede a remocao dos dados do servidor, ocasiona na sua desconexao

typedef struct no{
    char *telefone;
    struct sockaddr_in localizacao;
    int conectado;
    struct no *prox; 
    struct no *ant;
}usuario;

//mutex
pthread_mutex_t mutex;

//thread_args
typedef struct{
    int socket;
    usuario *lista;
    struct sockaddr_in myName;

}thread_arg,*ptr_thread_arg;

//thread_func
void *thread_cliente(void*);


/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_usuario(usuario **raiz,char *tel);
/*Adiciona o usuario no fim da lista, caso usuario ja exista chama ´put_online´*/
void adiciona_usuario(usuario **raiz,usuario add);
/*Define o usuario com ´tel´ como offline*/
int put_offline(usuario **raiz,char *tel);
/*Define o usuario com ´tel´ como online*/
int put_online(usuario **raiz,char *tel,struct sockaddr_in);
/*printa a lista*/
void print_lista(usuario *lista){
    while(lista!=NULL){
        printf("Telefone: %s\tStatus: %i\n",lista->telefone,lista->conectado);
        lista = lista->prox;
    }
}


int main(int argc, char *argv[])
{
    int namelen;
    int socket_conexao;
    int socket_thread;

    struct sockaddr_in cliente; 
    struct sockaddr_in servidor; 
    usuario *lista=NULL;

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
    bind(socket_conexao, (struct sockaddr *)&servidor, sizeof(servidor));
    if (socket_conexao < 0)
    {
       perror("ERRO - Bind()");
       printf("%i",errno);
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
        t_arg.lista = lista;
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
    usuario cliente;

    ptr_thread_arg thread_arg = (ptr_thread_arg)arg;

    int socket = thread_arg->socket;
    usuario *lista = thread_arg->lista;
    struct sockaddr_in whoami = thread_arg->myName; 

    //Recebimento da porta de escuta e telefone
    if(recv(socket,buffer_recebe,sizeof(buffer_recebe),0) == -1){
        perror("ERRO - Recv(porta,telefone");
        exit(errno);
    }
    msg[0]=strtok(buffer_recebe,";");
    msg[1]=strtok(NULL,";");

    whoami.sin_port = htons(atoi(msg[2]));
    cliente.localizacao = whoami;
    cliente.telefone = msg[1];
    cliente.conectado=1;
    //Atualizacao da lista de clientes
    pthread_mutex_trylock(&mutex);
        adiciona_usuario(&lista,cliente);
    pthread_mutex_unlock(&mutex);
    do{
        msg[1]=msg[0]=NULL;
        if(recv(socket,buffer_recebe,sizeof(buffer_recebe),0) == -1){
            fprintf(stderr,"ERRO - Recv(): %s, cliente id: %s",strerror(errno),cliente.telefone);
            break;
        }
        msg[0] = strtok(buffer_recebe,";");//comando
        msg[1] = strtok(NULL,";");//parametros comando

        if(strcmp(msg[0],GET) == 0){
            //Requisicao de informações

        }
        if(strcmp(msg[0],REMOVE) == 0){
            //Requisicao de remocao
            
        }
        

    }while(strcmp(msg[0],ENCERRAR) != 0);

    pthread_mutex_trylock(&mutex);
        put_offline(&lista,cliente.telefone);
    pthread_mutex_unlock(&mutex);
    close(socket);
}

int remove_usuario(usuario **raiz,char *tel){

    usuario *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->telefone,tel) == 0){
            if(aux->ant == NULL){
                *raiz = aux->prox;
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

int put_offline(usuario **raiz,char *tel){

    usuario *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->telefone,tel) == 0){
            aux->conectado=0;
            aux->localizacao.sin_addr.s_addr = INADDR_NONE;
            aux->localizacao.sin_port = 0;
            return 1;
        }else{
            aux = aux->prox;
        }
    }
    return 0;
}

/*Define o usuario com ´tel´ como online*/
int put_online(usuario **raiz,char *tel,struct sockaddr_in localizacao){

    usuario *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->telefone,tel) == 0){
            aux->conectado=1;
            aux->localizacao = localizacao;
            return 1;
        }else{
            aux = aux->prox;
        }
    }
    return 0;
}

/*Adiciona o usuario no fim da lista, caso usuario ja exista chama ´put_online´*/
void adiciona_usuario(usuario **raiz,usuario add){

    usuario *novo = (usuario *)malloc(sizeof(usuario));
    if(novo == NULL){
        perror("ERRO - malloc(novo)");
        exit(errno);
    }

    if(put_online(raiz,add.telefone,add.localizacao) == 1){
        return;
    }

    novo->telefone = add.telefone;
    novo->conectado = add.conectado;
    novo->localizacao = add.localizacao;
    novo->prox=NULL;
        
    if(*raiz == NULL){
        *raiz = novo;
        (*raiz)->ant = NULL;
    }else{
        usuario *aux = *raiz;
        while(aux->prox != NULL){
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant=aux;
    }
}