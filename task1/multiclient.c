#include "csapp.h"
#include <sys/time.h>
#include <time.h>

#define MAX_CLIENT 100

/* 성능 실험할 때 request 개수를 바꿔보려고 둔 값 */
#ifndef ORDER_PER_CLIENT
#define ORDER_PER_CLIENT 10
#endif

#define STOCK_NUM 10
#define BUY_SELL_MAX 10

#define WORKLOAD_MIXED 0
#define WORKLOAD_SHOW 1
#define WORKLOAD_WRITE 2

#ifndef WORKLOAD_MODE
#define WORKLOAD_MODE WORKLOAD_MIXED
#endif

#ifndef USE_USLEEP
#define USE_USLEEP 1
#endif

#ifndef SLEEP_USEC
#define SLEEP_USEC 1000000
#endif

#ifndef PRINT_RESPONSE
#define PRINT_RESPONSE 1
#endif

static double elapsed_time(struct timeval start, struct timeval end)
{
	return (double)(end.tv_sec - start.tv_sec) +
		   (double)(end.tv_usec - start.tv_usec) / 1000000.0;
}

/* 실험할 workload에 따라서 어떤 명령을 보낼지 정한다. */
static int choose_option(void)
{
	if (WORKLOAD_MODE == WORKLOAD_SHOW)
		return 0;
	if (WORKLOAD_MODE == WORKLOAD_WRITE)
		return rand() % 2 + 1;
	return rand() % 3;
}

int main(int argc, char **argv) 
{
	pid_t pids[MAX_CLIENT];
	int runprocess = 0, status, i;

	int clientfd, num_client;
	char *host, *port, buf[MAXLINE], tmp[3];
	rio_t rio;
	struct timeval start, end;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <host> <port> <client#>\n", argv[0]);
		exit(0);
	}

	host = argv[1];
	port = argv[2];
	num_client = atoi(argv[3]);
	if (num_client > MAX_CLIENT) {
		fprintf(stderr, "client#은 %d 이하로 입력해야 합니다\n", MAX_CLIENT);
		exit(0);
	}

	gettimeofday(&start, NULL);

/*	client 개수만큼 자식 프로세스를 만든다.	*/
	while(runprocess < num_client){
		//wait(&state);
		pids[runprocess] = fork();

		if(pids[runprocess] < 0)
			return -1;
		/*	자식 프로세스는 서버에 접속해서 명령을 여러 번 보낸다.	*/
		else if(pids[runprocess] == 0){
			if (PRINT_RESPONSE)
				printf("child %ld\n", (long)getpid());

			clientfd = Open_clientfd(host, port);
			Rio_readinitb(&rio, clientfd);
			srand((unsigned int) getpid());

			for(i=0;i<ORDER_PER_CLIENT;i++){
				int option = choose_option();
				
				if(option == 0){//show 명령
					strcpy(buf, "show\n");
				}
				else if(option == 1){//buy 명령
					int list_num = rand() % STOCK_NUM + 1;
					int num_to_buy = rand() % BUY_SELL_MAX + 1;//1~10

					strcpy(buf, "buy ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_buy);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				else if(option == 2){//sell 명령
					int list_num = rand() % STOCK_NUM + 1; 
					int num_to_sell = rand() % BUY_SELL_MAX + 1;//1~10
					
					strcpy(buf, "sell ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_sell);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				//strcpy(buf, "buy 1 2\n");
			
				Rio_writen(clientfd, buf, strlen(buf));
				// Rio_readlineb(&rio, buf, MAXLINE);
				Rio_readnb(&rio, buf, MAXLINE);
				if (PRINT_RESPONSE)
					Fputs(buf, stdout);

				if (USE_USLEEP)
					usleep(SLEEP_USEC);
			}

			Close(clientfd);
			exit(0);
		}
		/*	부모 프로세스에서는 아래에서 waitpid로 자식들을 기다린다.	*/
		/*else{
			for(i=0;i<num_client;i++){
				waitpid(pids[i], &status, 0);
			}
		}*/
		runprocess++;
	}
	for(i=0;i<num_client;i++){
		waitpid(pids[i], &status, 0);
	}

	gettimeofday(&end, NULL);
	printf("elapsed %.6f seconds\n", elapsed_time(start, end));


	/*clientfd = Open_clientfd(host, port);
	Rio_readinitb(&rio, clientfd);

	while (Fgets(buf, MAXLINE, stdin) != NULL) {
		Rio_writen(clientfd, buf, strlen(buf));
		Rio_readlineb(&rio, buf, MAXLINE);
		Fputs(buf, stdout);
	}

	Close(clientfd); //line:netp:echoclient:close
	exit(0);*/

	return 0;
}
