/*
 * stockclient.c
 * stock server에 명령을 하나씩 보내서 확인할 때 사용한다.
 */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    ssize_t n;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
	Rio_writen(clientfd, buf, strlen(buf));
	memset(buf, 0, MAXLINE);
	n = Rio_readnb(&rio, buf, MAXLINE);
	if (n <= 0)
	    break;
	Fputs(buf, stdout);
    }
    Close(clientfd);
    exit(0);
}
