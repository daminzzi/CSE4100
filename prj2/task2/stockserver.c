/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define SBUFSIZE 32
#define NTHREADS 16

typedef struct item {
    int id;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex; // protects access to the shared readcnt variable
    sem_t w; // controls access to the critical sections that access shared object
    struct item* left;
    struct item* right;
}item;

typedef struct {
    int* buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
}sbuf_t;

item* root;
sbuf_t sbuf; /*Shared buffer of connected descriptors*/
char str[MAXLINE];

item* insert(item* root, int id, int left_stock, int price);
item* fMin(item* root);
item* search(item* root, int id);
item* delete(item* root, int id);
void preorderPrint(item* root);
void stockTxt(item* root, FILE* fp);
void freeTree(item* root);

void show(int connfd, item* node);
void buy(int connfd, item* node, int ID, int n);
void sell(int connfd, item* node, int ID, int n);
void stockClient(int connfd);

void* thread(void* vargp);
void sbuf_init(sbuf_t* sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);

void echo(int connfd);

void sigint_handler(int signo) { // signal handler for sigint
    //printf("Terminate server\n");
    int r = remove("stock.txt"); // remove file
    if (r == -1) printf("Error : fail to remove stock.txt\n");
    else { // create stock.txt file
        FILE* fp2 = fopen("stock.txt", "w");
        if (fp2 == NULL) {
            perror("Failed to open the source");
        }
        else {
            stockTxt(root, fp2);
            fclose(fp2);
        }
    }
    freeTree(root);
    exit(1);
}

int main(int argc, char **argv) 
{
    Signal(SIGINT, sigint_handler); // check signal like ctrl + c

    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    int id, left, price;
    FILE* fp = fopen("stock.txt", "r");
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    if (fp == NULL) {
        printf("Error: stock.txt file does not exist.\n");
        return 0;
    }
    else {
        while (EOF != fscanf(fp, "%d %d %d\n", &id, &left, &price)) {
            root = insert(root, id, left, price);
        }
        fclose(fp);
    }

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    for (i = 0; i < NTHREADS; i++) /*Create worker threads*/
        Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);
    }
    exit(0);
}
/* $end echoserverimain */

item* insert(item* root, int id, int left_stock, int price) {
    if (root == NULL) {
        root = (item*)malloc(sizeof(item));
        root->id = id;
        root->left_stock = left_stock;
        root->price = price;
        root->readcnt = 0;
        root->left = NULL;
        root->right = NULL;
        Sem_init(&(root->mutex), 0, 1);
        Sem_init(&(root->w), 0, 1);
    }
    else {
        if (id < root->id) {
            root->left = insert(root->left, id, left_stock, price);
        }
        else {
            root->right = insert(root->right, id, left_stock, price);
        }
    }
    return root;
}
item* stockSearch(item* root, int key) {
    while (root != NULL && root->id != key) {
        if (root->id == key) {
            P(&(root->mutex)); // protect access for readcnt
            root->readcnt++; // increase the number of readers
            if ((root->readcnt) == 1) 
                P(&(root->w)); // protect access for left_stock
            V(&(root->mutex)); // unlock access for readcnt
        }
        else if (root->id > key) {
            root = root->left;
        }
        else {
            root = root->right;
        }
    }
    return root;
}

void stockTxt(item* root, FILE* fp) {
    if (root == NULL) return;
    stockTxt(root->left, fp);
    fprintf(fp, "%d %d %d\n", root->id, root->left_stock, root->price);
    stockTxt(root->right, fp);
    return;
}
void freeTree(item* root) {
    if (root == NULL) return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
    return;
}

void show(int connfd, item* root) {
    char buf[MAXLINE];
    if (root != NULL) {
        P(&(root->mutex));
        root->readcnt++;
        if ((root->readcnt) == 1) P(&(root->w));
        V(&(root->mutex));

        sprintf(buf, "%d %d %d\n", root->id, root->left_stock, root->price);
        strcat(str, buf);

        P(&(root->mutex));
        root->readcnt--;
        if ((root->readcnt) == 0)V(&(root->w));
        V(&(root->mutex));

        show(connfd, root->left);
        show(connfd, root->right);
    }
}

void buy(int connfd, item* root, int id, int n) {
    item* stock = stockSearch(root, id);
    char buf[MAXLINE];
    if ((stock->left_stock - n) >= 0) {
        P(&(stock->mutex));
        stock->readcnt--;
        if ((stock->readcnt) == 0) V(&(stock->w));
        V(&(stock->mutex));

        P(&(stock->w));
        stock->left_stock -= n;
        V(&(stock->w));
        strcpy(buf, "[buy] success\n\0");
        Rio_writen(connfd, buf, MAXLINE);
    }
    else {
        P(&(stock->mutex)); // readcnt
        stock->readcnt--; 
        if ((stock->readcnt) == 0)V(&(stock->w)); // left_stock
        V(&(stock->mutex)); // readcnt
        strcpy(buf, "Not enough left stock\n\0");
        Rio_writen(connfd, buf, MAXLINE);
    }
}

void sell(int connfd, item* root, int id, int n) {

    item* stock = stockSearch(root, id);
    char buf[MAXLINE];
    P(&(stock->mutex));
    stock->readcnt--;
    if ((stock->readcnt) == 0) V(&(stock->w));
    V(&(stock->mutex));

    P(&(stock->w));
    stock->left_stock += n;
    V(&(stock->w));
    strcpy(buf, "[sell] success\n\0");
    Rio_writen(connfd, buf, MAXLINE);
}

void stockClient(int connfd){
    int n;
    char buf[MAXLINE];
    rio_t rio;
    char cmd[10];
    int id, num;
    Rio_readinitb(&rio, connfd);

    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        cmd[0] = '\0'; id = 0; num = 0;
        sscanf(buf, "%s %d %d", cmd, &id, &num);
        printf("server received %d bytes(%d)\n", n, connfd);
        if (!strcmp(cmd, "show")) {
            for (int z = 0; z < MAXLINE; z++)str[z] = '\0';
            show(connfd, root);
            int slen = strlen(str);
            str[slen] = '\0';
            Rio_writen(connfd, str, MAXLINE);
        }
        else if (!strcmp(cmd, "buy")) {
            buy(connfd, root, id, num);
        }
        else if (!strcmp(cmd, "sell")) {
            sell(connfd, root, id, num);
        }
        else {
            Rio_writen(connfd, buf, MAXLINE);
        }
    }
}

void* thread(void* vargp) {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        //echo(connfd);
        stockClient(connfd);
        Close(connfd);
        printf("disconnected %d\n", connfd);
    }
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t* sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; /* Buffer holds max of n items */
    sp->front = sp->rear = 0; /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0); /* Initially, buf has 0 items */
}
/* Clean up buffer sp */
void sbuf_deinit(sbuf_t* sp)
{
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t* sp, int item)
{
    P(&sp->slots); /* Wait for available slot */
    P(&sp->mutex); /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->items); /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t* sp)
{
    int item;
    P(&sp->items); /* Wait for available item */
    P(&sp->mutex); /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->slots); /* Announce available slot */
    return item;
}