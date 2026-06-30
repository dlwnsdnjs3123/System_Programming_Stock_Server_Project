/*
 * stockserver.c
 * task1: select()를 이용한 stock server
 */
#include "csapp.h"

#define STOCK_FILE "stock.txt"
#define MAX_CLIENTS FD_SETSIZE

typedef struct item {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;          /* readcnt를 바꿀 때 사용 */
    sem_t w;              /* 이 종목을 읽고/쓸 때 사용 */
    struct item *left;
    struct item *right;
} item;

typedef struct {
    int fd;
    rio_t rio;
} client_t;

static item *root = NULL;
static client_t clients[MAX_CLIENTS];

static item *new_item(int ID, int left_stock, int price);
static item *insert_item(item *node, int ID, int left_stock, int price);
static item *find_item(item *node, int ID);
static void load_stock_file(void);
static void save_stock_file(void);
static void save_stock_file_rec(FILE *fp, item *node);
static void add_stock_line(char *response, int *idx, int ID, int left_stock, int price);
static void show_stock(char *response);
static void show_stock_rec(item *node, char *response, int *idx);
static void buy_stock(int ID, int amount, char *response);
static void sell_stock(int ID, int amount, char *response);
static int process_command(char *request, char *response);
static void sigint_handler(int sig);

/* stock.txt에서 읽은 한 줄을 트리 노드로 만든다. */
static item *new_item(int ID, int left_stock, int price)
{
    item *node = (item *)Malloc(sizeof(item));

    node->ID = ID;
    node->left_stock = left_stock;
    node->price = price;
    node->readcnt = 0;
    node->left = NULL;
    node->right = NULL;
    Sem_init(&node->mutex, 0, 1);
    Sem_init(&node->w, 0, 1);

    return node;
}

/* ID를 기준으로 이진 탐색 트리에 저장한다. */
static item *insert_item(item *node, int ID, int left_stock, int price)
{
    if (node == NULL)
        return new_item(ID, left_stock, price);

    if (ID < node->ID)
        node->left = insert_item(node->left, ID, left_stock, price);
    else if (ID > node->ID)
        node->right = insert_item(node->right, ID, left_stock, price);
    else {
        node->left_stock = left_stock;
        node->price = price;
    }

    return node;
}

/* buy, sell 명령에서 사용할 주식 ID를 찾는다. */
static item *find_item(item *node, int ID)
{
    while (node != NULL) {
        if (ID < node->ID)
            node = node->left;
        else if (ID > node->ID)
            node = node->right;
        else
            return node;
    }

    return NULL;
}

/* 서버가 시작될 때 stock.txt 내용을 메모리에 올린다. */
static void load_stock_file(void)
{
    FILE *fp;
    int ID, left_stock, price;

    fp = Fopen(STOCK_FILE, "r");
    while (fscanf(fp, "%d %d %d", &ID, &left_stock, &price) == 3)
        root = insert_item(root, ID, left_stock, price);
    Fclose(fp);
}

/* 서버가 종료될 때 현재 트리 내용을 다시 stock.txt에 저장한다. */
static void save_stock_file(void)
{
    FILE *fp = Fopen(STOCK_FILE, "w");

    save_stock_file_rec(fp, root);
    Fclose(fp);
}

/* 중위 순회로 저장하면 ID가 작은 순서대로 파일에 쓰인다. */
static void save_stock_file_rec(FILE *fp, item *node)
{
    if (node == NULL)
        return;

    save_stock_file_rec(fp, node->left);
    fprintf(fp, "%d %d %d\n", node->ID, node->left_stock, node->price);
    save_stock_file_rec(fp, node->right);
}

static void add_stock_line(char *response, int *idx, int ID, int left_stock, int price)
{
    char line[MAXLINE];
    int len;

    sprintf(line, "%d %d %d\n", ID, left_stock, price);
    len = strlen(line);
    if (*idx + len >= MAXLINE)
        return;

    strcpy(response + *idx, line);
    *idx += len;
}

static void show_stock(char *response)
{
    int idx = 0;

    response[0] = '\0';
    show_stock_rec(root, response, &idx);
}

static void show_stock_rec(item *node, char *response, int *idx)
{
    int ID, left_stock, price;

    if (node == NULL)
        return;

    show_stock_rec(node->left, response, idx);

    /* 수업의 readers-writers 방식으로 읽는 동안 쓰기를 막는다. */
    P(&node->mutex);
    node->readcnt++;
    if (node->readcnt == 1)
        P(&node->w);
    V(&node->mutex);

    ID = node->ID;
    left_stock = node->left_stock;
    price = node->price;

    P(&node->mutex);
    node->readcnt--;
    if (node->readcnt == 0)
        V(&node->w);
    V(&node->mutex);

    add_stock_line(response, idx, ID, left_stock, price);

    show_stock_rec(node->right, response, idx);
}

static void buy_stock(int ID, int amount, char *response)
{
    item *stock = find_item(root, ID);

    if (stock == NULL) {
        snprintf(response, MAXLINE, "Not enough left stocks\n");
        return;
    }

    /* buy는 left_stock을 바꾸므로 쓰기 작업처럼 처리한다. */
    P(&stock->w);
    if (stock->left_stock >= amount) {
        stock->left_stock -= amount;
        snprintf(response, MAXLINE, "[buy] success\n");
    }
    else {
        snprintf(response, MAXLINE, "Not enough left stocks\n");
    }
    V(&stock->w);
}

static void sell_stock(int ID, int amount, char *response)
{
    item *stock = find_item(root, ID);

    if (stock == NULL) {
        snprintf(response, MAXLINE, "[sell] success\n");
        return;
    }

    /* sell은 실패하지 않는다고 가정되어 있어서 개수만 더한다. */
    P(&stock->w);
    stock->left_stock += amount;
    snprintf(response, MAXLINE, "[sell] success\n");
    V(&stock->w);
}

static int process_command(char *request, char *response)
{
    char command[16];
    int ID, amount;

    memset(response, 0, MAXLINE);
    if (sscanf(request, "%15s", command) != 1)
        return 1;

    if (!strcmp(command, "show")) {
        show_stock(response);
        return 1;
    }
    if (!strcmp(command, "buy")) {
        sscanf(request, "%15s %d %d", command, &ID, &amount);
        buy_stock(ID, amount, response);
        return 1;
    }
    if (!strcmp(command, "sell")) {
        sscanf(request, "%15s %d %d", command, &ID, &amount);
        sell_stock(ID, amount, response);
        return 1;
    }
    if (!strcmp(command, "exit"))
        return 0;

    return 1;
}

static void sigint_handler(int sig)
{
    /* Ctrl+C로 서버를 끄면 변경된 재고를 파일에 반영한다. */
    save_stock_file();
    exit(0);
}

int main(int argc, char **argv)
{
    int listenfd, connfd, maxfd, maxi, i, nready;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    fd_set allset, rset;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, sigint_handler);
    Signal(SIGPIPE, SIG_IGN);
    load_stock_file();

    listenfd = Open_listenfd(argv[1]);
    maxfd = listenfd;
    maxi = -1;

    for (i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    while (1) {
        /* select가 rset을 바꾸기 때문에 매번 allset을 복사해서 사용한다. */
        rset = allset;
        nready = Select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

            Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                        client_port, MAXLINE, NI_NUMERICHOST | NI_NUMERICSERV);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd < 0) {
                    clients[i].fd = connfd;
                    Rio_readinitb(&clients[i].rio, connfd);
                    FD_SET(connfd, &allset);
                    if (connfd > maxfd)
                        maxfd = connfd;
                    if (i > maxi)
                        maxi = i;
                    break;
                }
            }

            if (i == MAX_CLIENTS)
                Close(connfd);

            if (--nready <= 0)
                continue;
        }

        for (i = 0; i <= maxi; i++) {
            int clientfd = clients[i].fd;

            if (clientfd < 0 || !FD_ISSET(clientfd, &rset))
                continue;

            char request[MAXLINE];
            char response[MAXLINE];
            ssize_t n = rio_readlineb(&clients[i].rio, request, MAXLINE);

            if (n <= 0) {
                Close(clientfd);
                FD_CLR(clientfd, &allset);
                clients[i].fd = -1;
            }
            else if (!process_command(request, response)) {
                Close(clientfd);
                FD_CLR(clientfd, &allset);
                clients[i].fd = -1;
            }
            /*
             * multiclient가 MAXLINE만큼 읽기 때문에 응답도 MAXLINE 크기로 보낸다.
             * 연결이 중간에 끊긴 경우를 처리하려고 여기서는 wrapper 대신 rio_writen을 썼다.
             */
            else if (rio_writen(clientfd, response, MAXLINE) != MAXLINE) {
                Close(clientfd);
                FD_CLR(clientfd, &allset);
                clients[i].fd = -1;
            }

            if (--nready <= 0)
                break;
        }
    }
}
