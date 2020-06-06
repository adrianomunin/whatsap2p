#include <stdio.h>
#include <stdio_ext.h>
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
#include <sys/sem.h>
#include <pthread.h>

//Comandos ao servidor
#define GETTEL "gettel"
#define GETLOC "getloc"
#define REMOVE "remove"
#define ENCERRAR "encerrar"
#define NOTFOUND "notfound"
//Comandos P2P
#define MSGPHOTO "msgphoto"
#define MSGTEXT "msgtext"

#define DEBUG 1

typedef struct noC{
    char telefone[20];
    char *nome;
    struct sockaddr_in localizacao;
    struct noC *prox;
    struct noC *ant;
}contato;

typedef struct noG{
    contato *membros;
    int qtd;
    char *nome;
    struct noG *prox;
    struct noG *ant;
}grupo;

//Thread mensagens setup
typedef struct{
    int socket_cliente;
    int socket_server;
    struct sockaddr_in server;
    contato *contatos;
    grupo *grupos;
}thread_arg,*ptr_thread_arg;

pthread_mutex_t mutex;

void *thread_msg(void*);

/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_contato(contato **raiz,char *tel);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
int adiciona_contato(contato **raiz,contato add);


/*Remove grupo da lista, retorna 1 se sucesso*/
int remove_grupo(grupo **raiz,char *nome);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
void cria_grupo(grupo **raiz,grupo add);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
int adiciona_membro(grupo **raiz,contato add,char *nome);
/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_membro(grupo **raiz,char *tel);
/*Procura contato na lista 1 se existe 0 se nao*/
int searchContato(contato **raiz,char *tel);
/*Procura grupo na lista 1 se existe 0 se nao*/
int searchGrupo(grupo **raiz,char *nome);


int main(int argc, char *argv[])
{
    int operacao;

    char buffer_envio[50],aux[50];
    char buffer_recebimento[50];
    char telefone[20];
    char path[100];
    char *msg[80];
    char *fileBuf;
    ssize_t size;
    FILE *fp;

    struct hostent *hostnm;
    int inetaddr;
    struct sockaddr_in server,myself;

    grupo *grupos=NULL;
    contato *contatos=NULL;

    contato contatoPraEnviar,contatoPraAdd;
    grupo grupoPraEnviar;

    int t_creat_res;
    pthread_t ptid;
    thread_arg t_arg;


    int countContatos=0;
    int countGrupos=0;
    int i,ret;

    if(pthread_mutex_init(&mutex,NULL)!=0){
        perror("ERRO - mutex_init()");
        exit(errno);
    } 

    int socket_recebe, socket_envia_servidor,socket_envia_cliente;
    int ip_recebimento, porta_recebimento;

    //Setup do socket de recebimento de mensagens
    if((socket_recebe = socket(PF_INET,SOCK_STREAM,0)) < 0){
        perror("ERRO - Socket(recv)");
        exit(errno);
    }

    myself.sin_addr.s_addr = INADDR_ANY;
    myself.sin_family = AF_INET;
    srand(time(NULL));
    do{

        porta_recebimento = rand()%65500;
        myself.sin_port = htons(porta_recebimento);
        
    }while(bind(socket_recebe,(struct sockaddr *)&myself,sizeof(myself)) != 0);

    if(listen(socket_recebe,1) != 0){
        perror("ERRO - Listen()");
        exit(errno);
    }



    if (argc < 3){
        printf("Use %s <host> <porta>\n",argv[0]);
        exit(-1);
    }        
    
    //Setup socket conexao ao servidor
    hostnm = gethostbyname(argv[1]);
    inetaddr = inet_addr(argv[1]);
        
        server.sin_family=AF_INET;
        server.sin_port = htons(atoi(argv[2]));
        if( hostnm == (struct hostent *) 0)
        {
            if(inetaddr == INADDR_NONE)
            {
                fprintf(stderr,"ERRO - nome do servidor\n");
                exit(-1);
            }
            server.sin_addr.s_addr=inetaddr;
        }else server.sin_addr.s_addr=*((unsigned long *)hostnm->h_addr_list[0]);

    if ((socket_envia_servidor = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("ERRO - Socket(snd)");
        exit(errno);
    }

    if ((connect(socket_envia_servidor, (struct sockaddr *)&server, sizeof(server))) < 0){
        perror("ERRO - Connect()");
        exit(errno);
    }

    printf("Informe seu telefone: ");
    __fpurge(stdin);
    fgets(telefone,sizeof(telefone),stdin);

    strcat(buffer_envio,telefone);
    strcat(buffer_envio,";");
    sprintf(aux,"%d",porta_recebimento);
    strcat(buffer_envio,aux);
    if (send(socket_envia_servidor, buffer_envio, sizeof(buffer_envio), 0) < 0){
        perror("ERRO - send(servidor)");
        exit(errno);
    }
    //Conectado ao servidor, lancando thread de mensagens

    t_arg.contatos = contatos;
    t_arg.grupos = grupos;
    t_arg.socket_cliente = socket_recebe;
    t_arg.socket_server = socket_envia_servidor;
    t_arg.server = server;

    t_creat_res = pthread_create(&ptid,NULL,&thread_msg,&t_arg);
    if(t_creat_res != 0){
        perror("ERRO - thread_create()");
        exit(t_creat_res);
    }

    do
    {
        printf("WhatsAp2p\n");
        printf("1 - Enviar Mensagem. \n");
        printf("2 - Criar Grupo. \n");
        printf("3 - Adicionar Contato. \n");
        printf("4 - Creditos.\n");
        printf("0 - Sair. \n>");
        __fpurge(stdin);
        scanf("%i", &operacao);
        //system("clear");
        strcpy(buffer_envio,"");
        strcpy(buffer_recebimento,"");
        switch (operacao)
        {
        case 1: //Enviar Msg
        {
            //Ordem de envio:
            //Tipo da mensagem - MSGTEXT ou MSGPHOTO
            //Tamanho da mensagem
            //A mensagem em si

            //Printo todos os contatos
            printf("\t\tNumero de contatos: %d\n\n",countContatos);
            i=0;
            while(contatos!=NULL){
                i++;
                printf("#%i - Nome: %s\tTelefone: %s\n",i,contatos->nome,contatos->telefone);
                contatos = contatos->prox;
            }
            printf("Qual contato?\n");
            __fpurge(stdin);
            scanf("%i",&operacao);
            if(operacao>i || operacao > countContatos || operacao<=0){
                printf("Selecao invalida\n");
                break;
            }
            //seleciono o contato requisitado
            while(contatos!=NULL){
                i--;
                if(i==0){
                    printf("SELECIONADO - Nome: %s\tTelefone: %s\n",contatos->nome,contatos->telefone);
                    contatoPraEnviar=*contatos;
                    break;
                }
                contatos = contatos->prox;
            }
            printf("O que deseja enviar para %s?\n",contatoPraEnviar.nome);
            printf("1 - Foto\n");
            printf("2 - Texto\n");
            printf("0 - Cancelar\n");
            __fpurge(stdin);
            scanf("%i",&operacao);
            if(operacao == 0)break;
            if(connect(socket_envia_cliente,(struct sockaddr *)&contatoPraEnviar.localizacao,sizeof(contatoPraEnviar.localizacao))){
                    perror("ERRO - connect2client");
                    break;
            }
            switch (operacao)
            {
            case 1:
                printf("Enviar Foto\n\n");
                //envio o tipo de mensagem
                if(send(socket_envia_cliente,MSGPHOTO,sizeof(MSGPHOTO),0) < 0){
                        perror("ERRO - send2client");
                        break;
                }
                //gero o caminho do arquivo solicitado
                getcwd(path,sizeof(path));
                strcat(path,"/");
                strcat(path,aux);

                fp = fopen(path,"rb");

                if(fp) {
                    //determino o tamanho do arquivo    
                    fseek(fp,0,SEEK_END);
                    size = ftell(fp);
                    fseek(fp,0,SEEK_SET);
                    fileBuf = malloc(size);
                    fread(fileBuf,size,1,fp);
                    fclose(fp);
                    //envio o tamanho do arquivo primeiro
                    sprintf(buffer_envio,"%lu",size);
                    if(send(socket_envia_cliente,buffer_envio,sizeof(buffer_envio),0) < 0){
                        perror("ERRO - send2client");
                        break;
                    }
                    //depois o arquivo em si
                    if((send(socket_envia_cliente,fileBuf,size,0)) < 0){
                        perror("ERRO - send2client");
                        break;
                    }
                    free(fileBuf);
                    printf("Arquivo enviado!");
                }else{ 
                    //Falha na leitura
                    perror("ERRO - read");
                    break;
                }
                break;
            
            case 2:
                printf("Enviar Texto\n\n");
                //envio o tipo de mensagem
                if(send(socket_envia_cliente,MSGTEXT,sizeof(MSGTEXT),0) < 0){
                        perror("ERRO - send2client");
                        break;
                }
                printf("Digite a mensagem\n>");
                __fpurge(stdin);
                fgets(buffer_envio,sizeof(buffer_envio),stdin);
                
                //envio o tamanho da mensagem
                sprintf(aux,"%lu",sizeof(buffer_envio));
                if(send(socket_envia_cliente,aux,sizeof(aux),0)<0){
                    perror("ERRO - send2client");
                    break;
                }
                //depois a mensagem em si
                if(send(socket_envia_cliente,buffer_envio,sizeof(buffer_envio),0)<0){
                    perror("ERRO - send2client");
                    break;
                }
                printf("Mensagem enviada!\n");
                break;
            
            case 0:
                break;
            
            default:
                printf("Selecao invalida\n");
                break;
            }

        }
        break;

        case 2: //Criar Grupo
        {
        }
        break;

        case 3: //Adc Contato
        {
            printf("Digite o telefone\n");
            __fpurge(stdin);
            fgets(telefone,sizeof(telefone),stdin);
            if (searchContato(&contatos,telefone)==1){
                printf("Contato existente!\n");
                break;
            }
            //gero a requisicao
            strcat(buffer_envio,GETLOC);
            strcat(buffer_envio,";");
            strcat(buffer_envio,telefone);
            #ifdef DEBUG
            printf("Comando enviado= %s\n\n",buffer_envio);
            #endif

            if(send(socket_envia_servidor,buffer_envio,sizeof(buffer_envio),0)<0){
                perror("ERRO - send2server");
                exit(errno);
            }

            if(recv(socket_envia_servidor,buffer_recebimento,sizeof(buffer_recebimento),0)<0){
                perror("ERRO - recvFromServer");
                exit(errno);
            }
            //tokenizo a resposta da requisicao, msg[0] tera ip/hostname, msg[1] tera porta
            //se telefone nao encontrado msg[0] = NOTFOUND
            msg[0]=strtok(buffer_recebimento,";\n");
            msg[1]=strtok(NULL,";\n");
            
            #ifdef DEBUG
            printf("Comando recebido= %s - %s\n\n",msg[0],msg[1]);
            #endif

            if(msg[0] == NULL || msg[1] == NULL){
                printf("ERRO - getloc received NULL\n");
                break;
            }
            if(strcmp(msg[0],NOTFOUND)==0){
                printf("Telefone nao encontrado!\n");
                break;
            }
            printf("Digite o nome\n");
            __fpurge(stdin);
            scanf("%s",contatoPraAdd.nome);
            memcpy(contatoPraAdd.telefone,telefone,sizeof(telefone));
            contatoPraAdd.localizacao.sin_addr.s_addr = inet_addr(msg[0]);
            contatoPraAdd.localizacao.sin_port = htons(atoi(msg[1]));
             if(adiciona_contato(&contatos,contatoPraAdd) == 0){
                 printf("Contato adicionado!\n");
                 countContatos+=1;
             }else{
                 printf("Contato ja existente!");
             }
        }
        break;

        case 4: //Creditos
        {
        }
        break;

        case 0:
            break;
        default:
            printf("Nao entendi o que voce quer!\n");
            break;
        }

    } while (operacao != 0);


    if(send(socket_envia_servidor,ENCERRAR,sizeof(ENCERRAR),0)<0){
        perror("ERRO - send(ENCERRAR)");
    }
    pthread_cancel(ptid);
    close(socket_recebe);
    close(socket_envia_servidor);
    close(socket_envia_cliente);
    pthread_mutex_destroy(&mutex);
    free(contatos);
    free(grupos);
}


void * thread_msg(void *arg){

    ptr_thread_arg thread_arg = (ptr_thread_arg)arg;

    int socket_cliente = thread_arg->socket_cliente;
    int socket_server = thread_arg->socket_server;
    contato *contatos = thread_arg->contatos;
    grupo *grupos = thread_arg->grupos;

    struct sockaddr_in cliente;
    int socket_accept,namelen;

    char *buffer_msg; //buffer dinamico para corpo da msg
    char buffer_comando[80];//buffer estatico para o comando: RCVMSG, RCVPHOTO
    char aux[80];
    ssize_t size;
    FILE *fp;
    char path[1000];
    char timestamp[100];
    char msgtype[80];
    time_t rawtime;
    struct tm * timeinfo;

    do{
        namelen = sizeof(cliente);
        if((socket_accept = accept(socket_cliente,(struct sockaddr *)&cliente,(socklen_t *)&namelen))== -1){
            perror("ERRO - accept(thread)");
            exit(errno);
        }

        //Conexao de um cliente aceita, recebendo tipo de msg, foto ou texto
        if(recv(socket_accept,buffer_comando,sizeof(size),0) == -1){
            perror("ERRO - recv(thread");
            exit(errno);
        }
        memcpy(msgtype,buffer_comando,sizeof(msgtype));

        //recebo o tamanho da mensagem
        if(recv(socket_accept,buffer_comando,sizeof(size),0) == -1){
            perror("ERRO - recv(thread");
            exit(errno);
        }
        size = atol(buffer_comando);
        buffer_msg = malloc(size);

        //recebo o corpo da mensagem
        if(recv(socket_accept,buffer_msg,sizeof(size),0) == -1){
            perror("ERRO - recv(thread");
            exit(errno);
        }

        //se for arquivo escrevo no disco
        if(strcmp(msgtype,MSGPHOTO) == 0){
            //gero a timestamp = [info de data/hora]telefone
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strcat(timestamp,"[");
            strcat(timestamp,asctime(timeinfo));
            strcat(timestamp,"]");

            //pergunto ao servidor telefone de quem enviou
            strcat(buffer_comando,GETTEL);
            strcat(buffer_comando,";");
            strcat(buffer_comando,inet_ntoa(cliente.sin_addr));
            strcat(buffer_comando,";");
            sprintf(aux,"%d",ntohs(cliente.sin_port));
            strcat(buffer_comando,aux);
            #ifdef DEBUG
            printf("Comando enviado= %s\n\n",buffer_comando);
            #endif
            if(send(socket_server,buffer_comando,sizeof(buffer_comando),0) < 0){
                perror("ERRO - send(thread)");
                exit(errno);
            }
            if(recv(socket_server,buffer_comando,sizeof(buffer_comando),0) == -1){
                perror("ERRO - recv(thread)");
                exit(errno);
            }
            #ifdef DEBUG
            printf("Comando recebido= %s\n\n",buffer_comando);
            #endif
            strcat(timestamp,buffer_comando);

            //buffermsg tera o retorno de um fread, verifico se houve erros
            if(strcmp(strerror(atoi(buffer_msg)),"Success") == 0){
                            getcwd(path,sizeof(path));
                            strcat(path,"/");
                            strcat(path,timestamp);
                            fp = fopen(path,"wb");
                            fwrite(buffer_msg,size,1,fp);
                            fclose(fp);
            }

        }

        //se for text exibo um notificacao e a msg na tela
        if(strcmp(msgtype,MSGTEXT)==0){
            printf("MENSAGEM RECEBIDA\n");
            

        }

        free(buffer_msg);
    
    }while(1);

}


int searchContato(contato **raiz,char *tel){

    contato *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->telefone,tel) == 0){
            return 1;
        }else{
            aux = aux->prox;
        }
    }
    return 0;
}

int searchGrupo(grupo **raiz,char *nome){

    grupo *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->nome,nome) == 0){
            return 1;
        }else{
            aux = aux->prox;
        }
    }
    return 0;
}

int remove_contato(contato **raiz,char *tel){

    contato *aux = *raiz;
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

int remove_grupo(grupo **raiz,char *nome){

    grupo *aux = *raiz;
    while(aux != NULL){
        if(strcmp(aux->nome,nome) == 0){
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

int adiciona_contato(contato **raiz,contato add){

    if(searchContato(raiz,add.telefone) == 1){
        return -1;
    }

    contato *novo = (contato *)malloc(sizeof(contato));
    if(novo == NULL){
        perror("ERRO - malloc(novo)");
        exit(errno);
    }

    strcpy(novo->telefone,add.telefone);
    novo->nome = add.nome;
    novo->localizacao = add.localizacao;
    novo->prox=NULL;
        
    if(*raiz == NULL){
        *raiz = novo;
        (*raiz)->ant = NULL;
    }else{
        contato *aux = *raiz;
        while(aux->prox != NULL){
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant=aux;
    }
    return 0;
}

int adiciona_membro(grupo **raiz,contato add,char *nome){

    contato *novo = (contato *)malloc(sizeof(contato));
    if(novo == NULL){
        perror("ERRO - malloc(novo)");
        exit(errno);
    }
    strcpy(novo->telefone,add.telefone);
    novo->nome = add.nome;
    novo->localizacao = add.localizacao;
    novo->prox=NULL;


    while(*raiz != NULL){

        if(strcmp((*raiz)->nome,nome) == 0){

            if(searchContato(&(*raiz)->membros,add.telefone) == 1){
                free(novo);
                return -1;
            }
            contato *mem = (*raiz)->membros;

            if(mem == NULL){
                mem = novo;
                mem->ant = NULL;
            }else{
                while(mem->prox != NULL){
                    mem = mem->prox;
                }
                mem->prox = novo;
                mem->prox->ant=mem;   
            }
            (*raiz)->qtd+=1;
            return 0;
        }else{
            (*raiz) = (*raiz)->prox;
        }
    }

}

void cria_grupo(grupo **raiz,grupo add){

    if(searchGrupo(raiz,add.nome) == 1){
        return;
    }

    grupo *novo = (grupo *)malloc(sizeof(grupo));
    if(novo == NULL){
        perror("ERRO - malloc(novo)");
        exit(errno);
    }
    novo->nome = add.nome;
    novo->membros = add.membros;
    novo->qtd = add.qtd;
    novo->prox=NULL;
        
    if(*raiz == NULL){
        *raiz = novo;
        (*raiz)->ant = NULL;
    }else{
        grupo *aux = *raiz;
        while(aux->prox != NULL){
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant=aux;
    }
}