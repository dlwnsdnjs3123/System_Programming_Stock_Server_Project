/*
 * stockserver.c
 * task2: pthread와 sbuf를 이용한 stock server
 */
#include "csapp.h"

#define STOCK_FILE "stock.txt"
#define NTHREADS 16
#define SBUFSIZE 1024

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
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;          /* buffer 배열 접근 보호 */
    sem_t slots;          /* 비어 있는 칸 개수 */
    sem_t items;          /* 들어 있는 connfd 개수 */
} sbuf_t;

static item *root = NULL;
static sbuf_t sbuf;

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
static void sbuf_init(sbuf_t *sp, int n);
static void sbuf_insert(sbuf_t *sp, int item);
static int sbuf_remove(sbuf_t *sp);
static void *worker(void *vargp);
static void service_client(int connfd);

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

    /* 여러 worker thread가 동시에 show를 할 수 있어서 readers-writers 방식을 사용한다. */
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

/* 수업의 producer-consumer 예제처럼 connfd를 담을 buffer를 초기화한다. */
static void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = (int *)Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = 0;
    sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

/* master thread가 accept한 connfd를 buffer에 넣는다. */
static void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % sp->n] = item;
    V(&sp->mutex);
    V(&sp->items);
}

/* worker thread가 처리할 connfd를 buffer에서 꺼낸다. */
static int sbuf_remove(sbuf_t *sp)
{
    int item;

    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % sp->n];
    V(&sp->mutex);
    V(&sp->slots);

    return item;
}

static void *worker(void *vargp)
{
    /* worker는 계속 살아 있으므로 detach로 자원 회수 문제를 줄인다. */
    Pthread_detach(Pthread_self());

    while (1) {
        int connfd = sbuf_remove(&sbuf);
        service_client(connfd);
        Close(connfd);
    }

    return NULL;
}

static void service_client(int connfd)
{
    rio_t rio;
    char request[MAXLINE];
    char response[MAXLINE];
    ssize_t n;

    Rio_readinitb(&rio, connfd);
    while ((n = rio_readlineb(&rio, request, MAXLINE)) > 0) {
        if (!process_command(request, response))
            break;
        /*
         * multiclient가 MAXLINE만큼 읽기 때문에 응답도 MAXLINE 크기로 보낸다.
         * 연결이 중간에 끊긴 경우를 처리하려고 여기서는 wrapper 대신 rio_writen을 썼다.
         */
        if (rio_writen(connfd, response, MAXLINE) != MAXLINE)
            break;
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd, i;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, sigint_handler);
    Signal(SIGPIPE, SIG_IGN);
    load_stock_file();
    sbuf_init(&sbuf, SBUFSIZE);

    /* 먼저 worker thread pool을 만들어 둔다. */
    for (i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, worker, NULL);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                    client_port, MAXLINE, NI_NUMERICHOST | NI_NUMERICSERV);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* master thread는 직접 처리하지 않고 worker에게 넘긴다. */
        sbuf_insert(&sbuf, connfd);
    }
}
