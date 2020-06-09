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
#define ENCERRAR "encerrar"
#define NOTFOUND "notfound"
#define NOTCONNECTED "notconnected"
#define OK "ok"
//Comandos P2P
#define MSGPHOTO "msgphoto"
#define MSGTEXT "msgtext"

#define DEBUG 1

#define TAM_BUFFER 250


typedef struct noC
{
    char telefone[20];
    char nome[50];
    struct sockaddr_in localizacao;
    struct noC *prox;
    struct noC *ant;
} contato;

typedef struct noG
{
    contato *membros;
    int qtd;
    char nome[50];
    struct noG *prox;
    struct noG *ant;
} grupo;


typedef struct noM{
    char msg[TAM_BUFFER];
    int qtd;
    contato remetente;
    struct tm timeinfo;
    struct noM *prox;
}mensagem;

//Thread mensagens setup
typedef struct
{
    int socket_server;
    int socket_recebe_cliente;
    struct sockaddr_in server;
} thread_arg, *ptr_thread_arg;

pthread_mutex_t mutex;

//lista de contatos e grupos
grupo *listaGrupos = NULL;
contato *listaContatos = NULL;
mensagem *listaMensagens = NULL;
int naoLida=0;
void *thread_msg(void *);

/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_contato(char *tel);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
int adiciona_contato(contato add);

/*Remove grupo da lista, retorna 1 se sucesso*/
int remove_grupo(char *nome);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
void cria_grupo(grupo add);
/*Adiciona o usuario no fim da lista, caso usuario ja exista nada eh feito*/
int adiciona_membro(contato add, char *nome);
/*Remove usuario da lista, retorna 1 se sucesso*/
int remove_membro(char *tel);
/*Procura contato na lista 1 se existe 0 se nao*/
int searchContato(contato *lista, char *tel);
/*Procura grupo na lista 1 se existe 0 se nao*/
int searchGrupo(char *nome);
/*Carrega para o arquivo a lista de contatos*/
int escrever_arq_contato(char telefone[]);
/*Carrega do arquivo a lista de contatos*/
int ler_arq_contato(char telefone[], int countContatos);
/*Carrega para o arquivo a lista de Grupos*/
int escrever_arq_grupo(char telefone[]);
/*Carrega do arquivo a lista de Grupos*/
int ler_arq_grupo(char telefone[], int countGrupos);
/*imprime informacoes do grupo*/
void print_grupo(char *nome);

int adiciona_msg(mensagem msg);
void print_msgs();
void selecionar_contato(contato *contatoPraEnviar, int countContatos);
void get_localizacao(contato *contatoPraEnviar, int socket_envia_servidor);
void enviar_foto(int socket_envia_cliente);
void enviar_texto(int socket_envia_cliente, char *buffer_envio);
int conecta_cliente(contato *contatoPraEnviar, char *telefone);
int listar_contatos();
void listar_grupos();
void selecionar_grupo(grupo *grupoPraEnviar);


int main(int argc, char *argv[])
{
    int operacao;
    int operacao_principal;

    char buffer_envio[TAM_BUFFER], aux[TAM_BUFFER];
    char buffer_recebimento[TAM_BUFFER];
    char telefone[20];
    char path[100];
    char *msg[TAM_BUFFER];
    //char *fileBuf;
    //ssize_t size;
    //FILE *fp;

    struct hostent *hostnm;
    int inetaddr;
    struct sockaddr_in server, myself;

    contato *auxContato;
    grupo *auxGrupo;

    contato contatoPraEnviar, contatoPraAdd;
    grupo grupoPraEnviar, grupoPraAdd;

    int t_creat_res;
    pthread_t ptid;
    thread_arg t_arg;

    int countContatos = 0;
    int countGrupos = 0;
    int i, ret;

    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        perror("ERRO - mutex_init()");
        exit(errno);
    }

    int socket_recebe_cliente, socket_envia_servidor, socket_envia_cliente;
    int ip_recebimento, porta_recebimento;
    int bindReturn;

    //Setup do socket de envio de mensagens ao cliente
    /*if((socket_envia_cliente = socket(PF_INET,SOCK_STREAM,0)) < 0){
        perror("ERRO - Socket(recv)");
        exit(errno);
    }*/
    //Setup do socket de recebimento de mensagens
    if ((socket_recebe_cliente = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERRO - Socket(recv)");
        exit(errno);
    }
    myself.sin_addr.s_addr = INADDR_ANY;
    myself.sin_family = AF_INET;
    srand(time(NULL));
    do
    {

        porta_recebimento = rand() % 65500;
        myself.sin_port = htons(porta_recebimento);
        bindReturn = bind(socket_recebe_cliente, (struct sockaddr *)&myself, sizeof(myself));
    } while (bindReturn != 0);

    if (listen(socket_recebe_cliente, 1) != 0)
    {
        perror("ERRO - Listen()");
        exit(errno);
    }

    if (argc < 3)
    {
        printf("Use %s <host> <porta>\n", argv[0]);
        exit(-1);
    }

    //Setup socket conexao ao servidor
    hostnm = gethostbyname(argv[1]);
    inetaddr = inet_addr(argv[1]);

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    if (hostnm == (struct hostent *)0)
    {
        if (inetaddr == INADDR_NONE)
        {
            fprintf(stderr, "ERRO - nome do servidor\n");
            exit(-1);
        }
        server.sin_addr.s_addr = inetaddr;
    }
    else
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr_list[0]);

    if ((socket_envia_servidor = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERRO - Socket(snd)");
        exit(errno);
    }

    if ((connect(socket_envia_servidor, (struct sockaddr *)&server, sizeof(server))) < 0)
    {
        perror("ERRO - Connect()");
        exit(errno);
    }

    printf("Informe seu telefone: ");
    __fpurge(stdin);
    fgets(telefone, sizeof(telefone), stdin);
    strcpy(telefone,strtok(telefone,"\n"));

    strcpy(buffer_envio, telefone);
    strcat(buffer_envio, ";");
    sprintf(aux, "%d", porta_recebimento);
    strcat(buffer_envio, aux);

    #ifdef DEBUG
    printf("Comando enviado: %s\n\n",buffer_envio);
    #endif
    if (send(socket_envia_servidor, buffer_envio, sizeof(buffer_envio), 0) < 0)
    {
        perror("ERRO - send(servidor)");
        exit(errno);
    }
    if(recv(socket_envia_servidor,buffer_recebimento,sizeof(buffer_recebimento),0) == -1){
        perror("ERRO - recv(servidor confirmation)");
        exit(errno);
    }
    
    #ifdef DEBUG
    printf("Comando recebido: %s\n\n",buffer_recebimento);
    #endif    

    //Falha no registro do servidor
    if(strcmp(buffer_recebimento,NOTCONNECTED)==0){
        errno=98;
        perror("ERRO - connect()");
        exit(errno);
    }

    //Conectado ao servidor, lancando thread de mensagens
    t_arg.socket_server = socket_envia_servidor;
    t_arg.socket_recebe_cliente = socket_recebe_cliente;
    t_arg.server = server;

    t_creat_res = pthread_create(&ptid, NULL, &thread_msg, &t_arg);
    if (t_creat_res != 0)
    {
        perror("ERRO - thread_create()");
        exit(t_creat_res);
    }

    countContatos = ler_arq_contato(telefone, countContatos);
    countGrupos = ler_arq_grupo(telefone, countGrupos);

    do
    {
        //system("clear");
        printf("\n\n    WhatsAp2p\n");
        printf("1 - Enviar Mensagem. \n");
        printf("2 - Enviar Foto. \n");
        printf("3 - Adicionar Contato. \n");
        printf("4 - Listar Contatos. \n");
        printf("5 - Criar Grupo. \n");
        printf("6 - Listar Grupo. \n");        
        printf("7 - Listar Mensagens. %i nao lidas\n",naoLida);
        printf("8 - Creditos.\n");
        printf("0 - Sair. \n>");
        __fpurge(stdin);
        scanf("%i", &operacao_principal);

        switch (operacao_principal)
        {
        case 1: //Enviar Msg

            //Ordem de envio:
            //Tipo da mensagem - MSGTEXT ou MSGPHOTO
            //Tamanho da mensagem
            //A mensagem em si

            printf("Enviar Mesnsagem Para:\n");
            printf("1 - Numero\n");
            printf("2 - Contato\n");
            printf("3 - Grupo\n");
            printf("0 - Cancelar\n");

            __fpurge(stdin);
            scanf("%i", &operacao);

            switch (operacao)
                {
                case 1:
                    break;
                case 2:
                    selecionar_contato(&contatoPraEnviar, countContatos);
                    get_localizacao(&contatoPraEnviar, socket_envia_servidor);
                    socket_envia_cliente = conecta_cliente(&contatoPraEnviar, telefone);
                    
                     printf("Digite a mensagem\n>");
                    __fpurge(stdin);
                    fgets(buffer_envio, sizeof(buffer_envio), stdin);

                    enviar_texto(socket_envia_cliente, buffer_envio);
                    close(socket_envia_cliente);                        
                    break;
                case 3:
                selecionar_grupo(&grupoPraEnviar);
                    printf("Mensagem sera enviada para o grupo %s\n", grupoPraEnviar.nome);
                    print_grupo(grupoPraEnviar.nome);
                    
                    auxContato = grupoPraEnviar.membros;

                    printf("Digite a mensagem\n>");
                    __fpurge(stdin);
                    fgets(buffer_envio, sizeof(buffer_envio), stdin);

                    while(auxContato != NULL){
                        get_localizacao(auxContato, socket_envia_servidor);
                        socket_envia_cliente = conecta_cliente(auxContato, telefone);
                        enviar_texto(socket_envia_cliente, buffer_envio);
                        close(socket_envia_cliente);
                        auxContato = auxContato->prox;
                    }  
                    break;                

            case 0:
                break;
            default:
                printf("Nao entendi o que voce quer!\n");
                break;
            }
            
            break;

        case 2: //Enviar foto
            printf("Enviar Foto Para:\n");
            printf("1 - Numero\n");
            printf("2 - Contato\n");
            printf("3 - Grupo\n");
            printf("0 - Cancelar\n");

            __fpurge(stdin);
            scanf("%i", &operacao);

            switch (operacao)
            {
            case 1:
                break;
            case 2:
                selecionar_contato(&contatoPraEnviar, countContatos);
                get_localizacao(&contatoPraEnviar, socket_envia_servidor);
                socket_envia_cliente = conecta_cliente(&contatoPraEnviar, telefone);
                enviar_foto(socket_envia_cliente);
                break;
            case 3:
                break;

            case 0:
                break;
            default:
                printf("Nao entendi o que voce quer!\n");
                break;
            }
            close(socket_envia_cliente);
            break;

        case 3: //Adc Contato
            printf("Digite o telefone\n");
            __fpurge(stdin);
            scanf("%s", contatoPraAdd.telefone);

            if (searchContato(listaContatos, telefone) == 1)
            {
                printf("Contato existente!\n");
                break;
            }

            printf("Digite o nome\n");
            __fpurge(stdin);
            scanf("%s", contatoPraAdd.nome);

            if (adiciona_contato(contatoPraAdd) == 0)
            {
                printf("Contato adicionado!\n");
                countContatos += 1;
            }
            else
            {
                printf("Contato ja existente!");
            }
            break;

        case 4: //Listar contatos
            printf("-----Contatos Cadastrados-----\n");
            countContatos = listar_contatos();
            printf("     Total de contatos: %i \n", countContatos);
            break;

        case 5: //Criar Grupo
            printf("Digite o Nome do Grupo\n");

            __fpurge(stdin);
            scanf("%s", grupoPraAdd.nome);
            cria_grupo(grupoPraAdd);

            do
            {
                print_grupo(grupoPraAdd.nome);
                printf("Selecione a opcao desejada\n");
                printf("1 - Adicionar contato ao grupo\n");
                printf("0 - Concluir\n");

                __fpurge(stdin);
                scanf("%i", &operacao);

                switch (operacao)
                {
                    int orden_contato;
                case 1:
                    //Printo todos os contatos
                    countContatos = listar_contatos();

                    printf("Qual contato?\n");
                    __fpurge(stdin);
                    scanf("%i", &orden_contato);

                    if (/*operacao>i || */ orden_contato > countContatos || orden_contato <= 0)
                    {
                        printf("Selecao invalida\n");
                        break;
                    }

                    //seleciono o contato requisitado

                    auxContato = listaContatos;
                    while (auxContato != NULL)
                    {
                        orden_contato--;
                        if (orden_contato == 0)
                        {
                            printf("SELECIONADO - Nome: %s\tTelefone: %s\n", auxContato->nome, auxContato->telefone);
                            strcpy(contatoPraAdd.nome, auxContato->nome);
                            strcpy(contatoPraAdd.telefone, auxContato->telefone);

                            if (adiciona_membro(contatoPraAdd, grupoPraAdd.nome) == 1)
                                printf("\n   *Contato ja cadastrado no grupo*\n\n");
                            break;
                        }
                        auxContato = auxContato->prox;
                    }
                    break;

                case 0:
                    break;
                default:
                    printf("Nao entendi o que voce quer!\n");
                    break;
                }

            } while (operacao != 0);
            break;

        case 6: //Listar Grupos
            listar_grupos();
            break;

        case 7: //Listar mensagens
        {
            printf("-----Mensagens Recebidas-----\n");
            print_msgs();
            
        }
        break;

        case 8://Creditos
        {
            printf("Feito com sangue e suor por: \nAdriano Munin - @adrianomunin\nFabio Irokawa - @fabioirokawa\nIago Lourenco - @iaglourenco\nLucas Coutinho - @lucasrcoutinho\n");
            printf("Mais informacoes em: https://github.com/adrianomunin/whatsap2p\n\n");
        }

        case 0:
            break;
        default:
            printf("Nao entendi o que voce quer!\n");
            break;
        }

    } while (operacao_principal != 0);

    escrever_arq_contato(telefone);
    escrever_arq_grupo(telefone);

    if (send(socket_envia_servidor, ENCERRAR, sizeof(ENCERRAR), 0) < 0)
    {
        perror("ERRO - send(ENCERRAR)");
    }
    pthread_cancel(ptid);

    close(socket_envia_servidor);
    //close(socket_envia_cliente);
    close(socket_recebe_cliente);
    pthread_mutex_destroy(&mutex);
    free(listaContatos);
    free(listaGrupos);
    free(listaMensagens);
}

void *thread_msg(void *arg)
{

    ptr_thread_arg thread_arg = (ptr_thread_arg)arg;

    int socket_server = thread_arg->socket_server;
    int socket_recebe = thread_arg->socket_recebe_cliente;
    struct sockaddr_in cliente;
    int socket_accept, namelen;

    char *buffer_msg;        //buffer dinamico para corpo da msg
    char buffer_comando[TAM_BUFFER]; //buffer estatico para o comando: RCVMSG, RCVPHOTO
    char aux[TAM_BUFFER];
    ssize_t size;
    FILE *fp;
    char path[1000];
    char timestamp[100];
    char telefone[20];
    char msgtype[TAM_BUFFER];
    time_t rawtime;
    struct tm *timeinfo;
    mensagem msgRecebida;

    do
    {
        buffer_msg = NULL;
        strcpy(buffer_comando, "");
        strcpy(path, "");
        strcpy(msgtype, "");
        strcpy(timestamp, "");

        namelen = sizeof(cliente);
        if ((socket_accept = accept(socket_recebe, (struct sockaddr *)&cliente, (socklen_t *)&namelen)) == -1)
        {
            perror("ERRO - accept(thread)");
            exit(errno);
        }

        //Conexao de um cliente aceita, recebo o telefone do remetente
        if (recv(socket_accept, buffer_comando, sizeof(buffer_comando), 0) == -1)
        {
            perror("ERRO - recv(thread");
            exit(errno);
        }
        strcpy(telefone, buffer_comando);
        // recebendo tipo de msg, foto ou texto
        if (recv(socket_accept, buffer_comando, sizeof(buffer_comando), 0) == -1)
        {
            perror("ERRO - recv(thread");
            exit(errno);
        }
        memcpy(msgtype, buffer_comando, sizeof(msgtype));

        //recebo o tamanho da mensagem
        if (recv(socket_accept, buffer_comando, sizeof(buffer_comando), 0) == -1)
        {
            perror("ERRO - recv(thread");
            exit(errno);
        }
        size = atol(buffer_comando);

        //se for arquivo escrevo no disco
        if (strcmp(msgtype, MSGPHOTO) == 0)
        {

            buffer_msg = malloc(size);
            //recebo o corpo da mensagem
            if (recv(socket_accept, buffer_msg, size, 0) == -1)
            {
                perror("ERRO - recv(thread");
                exit(errno);
            }

            //gero a timestamp = [info de data/hora]telefone
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strcpy(timestamp, "[");
            strcat(timestamp, asctime(timeinfo));
            strcat(timestamp, "]");
            strcat(timestamp, "[");
            strcat(timestamp, telefone);
            strcat(timestamp, "]");

            //buffermsg tera o retorno de um fread, verifico se houve erros
            if (strcmp(strerror(atoi(buffer_msg)), "Success") == 0)
            {
                getcwd(path, sizeof(path));
                strcat(path, "/");
                strcat(path, timestamp);
                fp = fopen(path, "wb");
                fwrite(buffer_msg, size, 1, fp);
                fclose(fp);
                
                strcpy(msgRecebida.msg,"FOTO");
                strcpy(msgRecebida.remetente.telefone,telefone);
                adiciona_msg(msgRecebida);
            }
        }

        //se for text exibo um notificacao e a msg na tela
        if (strcmp(msgtype, MSGTEXT) == 0)
        {

            buffer_msg = (char *)malloc(size);
            //recebo o corpo da mensagem
            if (recv(socket_accept, buffer_msg, size, 0) == -1)
            {
                perror("ERRO - recv(thread");
                exit(errno);
            }

            strcpy(msgRecebida.msg,buffer_msg);
            strcpy(msgRecebida.remetente.telefone,telefone);
            adiciona_msg(msgRecebida);
        }

        close(socket_accept);
        free(buffer_msg);

    } while (1);
}

int searchContato(contato *lista, char *tel)
{

    contato *aux = lista;
    while (aux != NULL)
    {
        if (strcmp(aux->telefone, tel) == 0)
        {
            return 1;
        }
        else
        {
            aux = aux->prox;
        }
    }
    return 0;
}

int searchGrupo(char *nome)
{

    grupo *aux = listaGrupos;
    while (aux != NULL)
    {
        if (strcmp(aux->nome, nome) == 0)
        {
            return 1;
        }
        else
        {
            aux = aux->prox;
        }
    }
    return 0;
}

int remove_contato(char *tel)
{

    contato *aux = listaContatos;
    while (aux != NULL)
    {
        if (strcmp(aux->telefone, tel) == 0)
        {
            if (aux->ant == NULL)
            {
                listaContatos = aux->prox;
                free(aux);
            }
            else
            {
                aux->ant->prox = aux->prox;
                free(aux);
            }
            return 1;
        }
        else
        {
            aux = aux->prox;
        }
    }
    return 0;
}

int remove_grupo(char *nome)
{

    grupo *aux = listaGrupos;
    while (aux != NULL)
    {
        if (strcmp(aux->nome, nome) == 0)
        {
            if (aux->ant == NULL)
            {
                listaGrupos = aux->prox;
                free(aux);
            }
            else
            {
                aux->ant->prox = aux->prox;
                free(aux);
            }
            return 1;
        }
        else
        {
            aux = aux->prox;
        }
    }
    return 0;
}

int adiciona_contato(contato add)
{

    if (searchContato(listaContatos, add.telefone) == 1)
    {
        return -1;
    }

    contato *novo = (contato *)malloc(sizeof(contato));
    if (novo == NULL)
    {
        perror("ERRO - malloc(novo)");
        exit(errno);
    }

    strcpy(novo->telefone, add.telefone);
    strcpy(novo->nome, add.nome);
    novo->prox = NULL;

    if (listaContatos == NULL)
    {
        listaContatos = novo;
        listaContatos->ant = NULL;
    }
    else
    {
        contato *aux = listaContatos;
        while (aux->prox != NULL)
        {
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant = aux;
    }
    return 0;
}

int adiciona_membro(contato add, char *nome)
{
    contato *novo = (contato *)malloc(sizeof(contato));
    if (novo == NULL)
    {
        perror("ERRO - malloc(novo)");
        exit(errno);
    }
    strcpy(novo->telefone, add.telefone);
    strcpy(novo->nome, add.nome);
    novo->prox = NULL;

    grupo *aux = listaGrupos;

    while (aux != NULL)
    {

        if (strcmp(aux->nome, nome) == 0)
        {
            if (searchContato(aux->membros, add.telefone) == 1)
            {
                return 1;
            }

            if (aux->membros == NULL)
            {
                aux->membros = novo;
                aux->membros->ant = NULL;
            }
            else
            {
                contato *mem = aux->membros;
                while (mem->prox != NULL)
                {
                    mem = mem->prox;
                    __fpurge(stdout);
                }
                mem->prox = novo;
                mem->prox->ant = mem;
            }
            aux->qtd += 1;
            return 0;
        }
        else
        {
            aux = aux->prox;
        }
    }
}

void cria_grupo(grupo add)
{

    if (searchGrupo(add.nome) == 1)
    {
        return;
    }

    grupo *novo = (grupo *)malloc(sizeof(grupo));
    if (novo == NULL)
    {
        perror("ERRO - malloc(novo)");
        exit(errno);
    }
    strcpy(novo->nome, add.nome);
    novo->membros = NULL;
    novo->qtd = 0;
    novo->prox = NULL;

    if (listaGrupos == NULL)
    {
        listaGrupos = novo;
        listaGrupos->ant = NULL;
    }
    else
    {
        grupo *aux = listaGrupos;
        while (aux->prox != NULL)
        {
            aux = aux->prox;
        }
        aux->prox = novo;
        aux->prox->ant = aux;
    }
}

int escrever_arq_contato(char telefone[])
{
    contato *pont_aux = listaContatos;
    contato struct_arquivada;

    FILE *arq;
    char nome_arquivo[50];
    strcpy(nome_arquivo, "C");
    strcat(nome_arquivo, telefone);
    strcat(nome_arquivo, ".dat");

    //abrindo ou criando arquivo que possui como nome o telefone do dono
    arq = fopen(nome_arquivo, "wb");
    if (arq == NULL)
    {
        perror("ERRO - fopen");
        exit(errno);
    }

    while (pont_aux != NULL)
    {
        //Salvando conteudo de um no da lista ligada na variavel "struct_arquivada"
        struct_arquivada.ant = pont_aux->ant;
        struct_arquivada.localizacao = pont_aux->localizacao;
        struct_arquivada.prox = pont_aux->prox;
        strcpy(struct_arquivada.nome, pont_aux->nome);
        strcpy(struct_arquivada.telefone, pont_aux->telefone);

        //Escrevendo no dentro do arquivo
        fwrite(&struct_arquivada, sizeof(struct_arquivada), 1, arq);
        pont_aux = pont_aux->prox;
    }

    printf("Contatos guardados com SUCESSO! \n");
    fclose(arq);
}

int ler_arq_contato(char telefone[], int countContatos)
{
    contato *aux;
    contato struct_arquivada;

    FILE *arq;
    char nome_arquivo[50];
    strcpy(nome_arquivo, "C");
    strcat(nome_arquivo, telefone);
    strcat(nome_arquivo, ".dat");

    ////abrindo arquivo que possui como nome o telefone do dono
    arq = fopen(nome_arquivo, "rb");
    if (arq == NULL)
    {
        //Se o arquivo solicitado nao existir(1 execucao), entao criaremos um
        arq = fopen(nome_arquivo, "ab");
        if (arq == NULL)
        {
            perror("ERRO - fopen");
            exit(errno);
        }
        fclose(arq);

        //Abrindo arquivo criado em modo leitura binaria
        arq = fopen(nome_arquivo, "rb");
        if (arq == NULL)
        {
            perror("ERRO - fopen - criacao");
            exit(errno);
        }
    }

    while (fread(&struct_arquivada, sizeof(struct_arquivada), 1, arq))
    {
        printf("Nome: %s \n", struct_arquivada.nome);
        printf("Telefone: %s \n", struct_arquivada.telefone);

        //Adicionando contatos lido do arquivo atravez da funcao criada adiciona_contato
        if (adiciona_contato(struct_arquivada) == 0)
        {
            printf("Contato adicionado!\n");
            countContatos += 1;
        }
    }

    printf("Contatos carregados com SUCESSO! \n");

    fclose(arq);
    return countContatos;
}

int escrever_arq_grupo(char telefone[])
{
    grupo *aux_grupo = listaGrupos;
    grupo struct_grupo;

    contato *aux_contato = listaContatos;
    contato struct_contato;

    FILE *arq;
    char nome_arquivo[50];
    strcpy(nome_arquivo, "G");
    strcat(nome_arquivo, telefone);
    strcat(nome_arquivo, ".dat");

    //abrindo ou criando arquivo que possui como nome o telefone do dono
    arq = fopen(nome_arquivo, "wb");
    if (arq == NULL)
    {
        perror("ERRO - fopen");
        exit(errno);
    }

    while (aux_grupo != NULL)
    {
        struct_grupo.ant = aux_grupo->ant;
        struct_grupo.prox = aux_grupo->prox;
        struct_grupo.qtd = aux_grupo->qtd;
        strcpy(struct_grupo.nome, aux_grupo->nome);

        fwrite(&struct_grupo, sizeof(struct_grupo), 1, arq);

        aux_contato = aux_grupo->membros;

        while (aux_contato != NULL)
        {
            struct_contato.ant = aux_contato->ant;
            struct_contato.prox = aux_contato->prox;
            struct_contato.localizacao = aux_contato->localizacao;
            strcpy(struct_contato.nome, aux_contato->nome);
            strcpy(struct_contato.telefone, aux_contato->telefone);

            fwrite(&struct_contato, sizeof(struct_contato), 1, arq);
            aux_contato = aux_contato->prox;
        }

        aux_grupo = aux_grupo->prox;
    }

    printf("Grupos guardados com SUCESSO! \n");
    fclose(arq);
}

int ler_arq_grupo(char telefone[], int countGrupos)
{
    grupo *aux_grupo = listaGrupos;
    grupo struct_grupo;

    contato *aux_contato = listaContatos;
    contato struct_contato;

    FILE *arq;
    char nome_arquivo[50];
    strcpy(nome_arquivo, "G");
    strcat(nome_arquivo, telefone);
    strcat(nome_arquivo, ".dat");

    ////abrindo arquivo que possui como nome o telefone do dono
    arq = fopen(nome_arquivo, "rb");
    if (arq == NULL)
    {
        //Se o arquivo solicitado nao existir(1 execucao), entao criaremos um
        arq = fopen(nome_arquivo, "ab");
        if (arq == NULL)
        {
            perror("ERRO - fopen");
            exit(errno);
        }
        fclose(arq);

        //Abrindo arquivo criado em modo leitura binaria
        arq = fopen(nome_arquivo, "rb");
        if (arq == NULL)
        {
            perror("ERRO - fopen - criacao");
            exit(errno);
        }
        }   

    while (fread(&struct_grupo, sizeof(struct_grupo), 1, arq))
    {
        #ifdef DEBUG
        printf("Nome do Grupo: %s \n", struct_grupo.nome);
        #endif
        
        cria_grupo(struct_grupo);

        if (struct_grupo.qtd != 0)
        {
            do
            {
                fread(&struct_contato, sizeof(struct_contato), 1, arq);
                adiciona_membro(struct_contato, struct_grupo.nome);
            } while (struct_contato.prox != NULL);
        }

        countGrupos++;
    }

    printf("Grupos carregados com SUCESSO! \n");

    fclose(arq);
    return countGrupos;
}
int adiciona_msg(mensagem msg){

    mensagem *novo = (mensagem *)malloc(sizeof(contato));
    if (novo == NULL)
    {
        perror("ERRO - malloc(novo)");
        exit(errno);
    }

    strcpy(novo->msg,msg.msg);
    novo->remetente = msg.remetente;
    novo->timeinfo = msg.timeinfo;
    
    if (listaMensagens == NULL)
    {
        listaMensagens = novo;
    }
    else
    {
        mensagem *aux = listaMensagens;
        while (aux->prox != NULL)
        {
            aux = aux->prox;
        }
        aux->prox = novo;
    }
    naoLida+=1;
    return 0;
}

void print_msgs(){

    mensagem *aux=listaMensagens;
    while(aux != NULL){
        printf("Telefone %s\tMensagem: %s\n",aux->remetente.telefone,aux->msg);
        aux = aux->prox;
    }
    naoLida=0;
}

void print_grupo(char *nome){
    grupo *aux = listaGrupos;

    while (aux != NULL)
    {
        if (strcmp(aux->nome, nome) == 0)
        {
            printf("--------Nome do grupo: %s---------\n", aux->nome);
            printf("\tQuantidade de membros %i\n\n", aux->qtd);
            __fpurge(stdout);

            contato *aux2 = aux->membros;
            while (aux2 != NULL)
            {
                printf("Nome %s Telefone: %s\n", aux2->nome, aux2->telefone);
                __fpurge(stdout);
                aux2 = aux2->prox;
            }
            printf("\n");
        }
        aux = aux->prox;
    }
}

void selecionar_contato(contato *contatoPraEnviar, int countContatos){
            contato *auxContato;
            int i = 0;
            int operacao;
            //Printo todos os contatos
            printf("\t\tNumero de contatos: %d\n\n", countContatos);
            
            auxContato = listaContatos;
            while (auxContato != NULL)
            {
                i++;
                printf("#%i - Nome: %s\tTelefone: %s\n", i, auxContato->nome, auxContato->telefone);
                auxContato = auxContato->prox;
            }
            printf("Qual contato?\n");
            __fpurge(stdin);
            scanf("%i", &operacao);
            if (operacao > i || operacao > countContatos || operacao <= 0)
            {
                printf("Selecao invalida\n");
                //break;
                return;
            }
            //seleciono o contato requisitado
            auxContato = listaContatos;
            while (auxContato != NULL)
            {
                operacao--;
                if (operacao == 0)
                {
                    printf("SELECIONADO - Nome: %s\tTelefone: %s\n", auxContato->nome, auxContato->telefone);
                    strcpy(contatoPraEnviar->nome, auxContato->nome);
                    strcpy(contatoPraEnviar->telefone, auxContato->telefone);
                    //break;
                    return;
                }
                auxContato = auxContato->prox;
            }
}

void get_localizacao(contato *contatoPraEnviar, int socket_envia_servidor){
            char buffer_envio[TAM_BUFFER];
            char buffer_recebimento[TAM_BUFFER];
            char *msg[TAM_BUFFER];

    strcpy(buffer_envio, GETLOC);
    strcat(buffer_envio, ";");
    strcat(buffer_envio, contatoPraEnviar->telefone);
#ifdef DEBUG
    printf("Comando enviado= %s\n\n", buffer_envio);
#endif

    if (send(socket_envia_servidor, buffer_envio, sizeof(buffer_envio), 0) < 0)
    {
        perror("ERRO - send2server");
        exit(errno);
    }

    if (recv(socket_envia_servidor, buffer_recebimento, sizeof(buffer_recebimento), 0) < 0)
    {
        perror("ERRO - recvFromServer");
        exit(errno);
    }
    //tokenizo a resposta da requisicao, msg[0] tera ip/hostname, msg[1] tera porta
    //se telefone nao encontrado msg[0] = NOTFOUND
    msg[0] = strtok(buffer_recebimento, ";\n");
    msg[1] = strtok(NULL, ";\n");

#ifdef DEBUG
    printf("Comando recebido= %s - %s\n\n", msg[0], msg[1]);
#endif

    if (msg[0] == NULL || msg[1] == NULL)
    {
        printf("ERRO - getloc received NULL\n");
        //break;
        return;
    }
    if (strcmp(msg[0], NOTFOUND) == 0)
    {
        printf("Telefone offline ou inexistente!\n");
        //break;
        return;
    }

    contatoPraEnviar->localizacao.sin_addr.s_addr = inet_addr(msg[0]);
    contatoPraEnviar->localizacao.sin_port = htons(atoi(msg[1]));
    contatoPraEnviar->localizacao.sin_family = AF_INET;
}

void enviar_texto(int socket_envia_cliente, char *buffer_envio){
                char aux[TAM_BUFFER];
                int ret;    

                printf("Enviar Texto\n\n");
                //envio o tipo de mensagem
                if (send(socket_envia_cliente, MSGTEXT, sizeof(aux), 0) < 0)
                {
                    perror("ERRO - send2client");
                    //break;
                    return;
                }
                
                //envio o tamanho da mensagem
                sprintf(aux, "%lu", strlen(buffer_envio));
                if (send(socket_envia_cliente, aux, sizeof(aux), 0) < 0)
                {
                    perror("ERRO - send2client");
                    //break;
                    return;
                }
                //depois a mensagem em si

    if ((ret = send(socket_envia_cliente, buffer_envio, strlen(buffer_envio), 0)) < 0)
    {
        perror("ERRO - send2client");
        //break;
        return;
    }
    printf("Mensagem enviada! %i bytes\n", ret);
}

void enviar_foto(int socket_envia_cliente){
    char buffer_envio[TAM_BUFFER];
    char aux[TAM_BUFFER];
    char path[1000];
    char *fileBuf;
    ssize_t size;
    FILE *fp;

    printf("Enviar Foto\n\n");
    printf("Digite o nome do arquivo\n>");
    __fpurge(stdin);
    scanf("%s", aux);
    //envio o tipo de mensagem
    if (send(socket_envia_cliente, MSGPHOTO, sizeof(buffer_envio), 0) < 0)
    {
        perror("ERRO - send2client");
        //break;
        return;
    }
    //gero o caminho do arquivo solicitado
    getcwd(path, sizeof(path));
    strcat(path, "/");
    strcat(path, aux);
    fp = fopen(path, "rb");
    if (fp)
    {
        //determino o tamanho do arquivo
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        fileBuf = malloc(size);
        fread(fileBuf, size, 1, fp);
        fclose(fp);
        //envio o tamanho do arquivo primeiro
        sprintf(buffer_envio, "%lu", size);
        if (send(socket_envia_cliente, buffer_envio, sizeof(buffer_envio), 0) < 0)
        {
            perror("ERRO - send2client");
            //break;
            return;
        }
        //depois o arquivo em si
        if ((send(socket_envia_cliente, fileBuf, size, 0)) < 0)
        {
            perror("ERRO - send2client");
            //break;
            return;
        }
        free(fileBuf);
        printf("Arquivo enviado!");
    }
    else
    {
        //Falha na leitura
        perror("ERRO - read");
        //break;
        return;
    }
}

int conecta_cliente(contato *contatoPraEnviar, char *telefone){//Conecta com cliente e envia informacoes do transmissor 
    int socket_envia_cliente;
    
    
    if ((socket_envia_cliente = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERRO - Socket(recv)");
        exit(errno);
    }
    if (connect(socket_envia_cliente, (struct sockaddr *)&contatoPraEnviar->localizacao, sizeof(contatoPraEnviar->localizacao)))
    {
        perror("ERRO - connect2client");
        //break;
    }
    if (send(socket_envia_cliente, telefone, sizeof(telefone), 0) < 0)
    {
        perror("ERRO - send2client");
        //break;
    }
    return socket_envia_cliente;
}

int listar_contatos()
{ //Retorna a quantidade de contatos
    contato *auxContato;
    int i = 0;
    //Printo todos os contatos
    auxContato = listaContatos;
    while (auxContato != NULL)
    {
        i++;
        printf("#%i - Nome: %s\tTelefone: %s\n", i, auxContato->nome, auxContato->telefone);
        auxContato = auxContato->prox;
    }
    return i;
}

void listar_grupos()
{
    grupo *aux = listaGrupos;
    while(aux != NULL){
        print_grupo(aux->nome);
        aux = aux->prox;
    }
}

void selecionar_grupo(grupo *grupoPraEnviar){
    int operacao, i;

    grupo *auxGrupo = listaGrupos;
    i = 0;
    while(auxGrupo != NULL){
        i++;
        printf("#%i - Nome do grupo: %s\n", i, auxGrupo->nome);
        auxGrupo = auxGrupo->prox;
    }

    printf("Qual Grupo?\n");
    __fpurge(stdin);
    scanf("%i", &operacao);
    if (operacao > i || operacao <= 0)
    {
        printf("Selecao invalida\n");
        //break;
        return;
    }

    auxGrupo = listaGrupos;
    while (auxGrupo != NULL)
    {
        operacao--;
        if (operacao == 0)
        {
            //printf("SELECIONADO - Grupo: %s\n", auxGrupo->nome);
            grupoPraEnviar->membros = auxGrupo->membros;
            strcpy(grupoPraEnviar->nome, auxGrupo->nome);

            //break;
            return;
        }
        auxGrupo = auxGrupo->prox;
    }     
}