/* 
 *
 * Arduino Control With Server
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#define BUF_SIZ 100
#define MAX_CLNT 10 
#define LOG_SIZ 30
#define NEAR_POINT 10
#define DARK_POINT 400

void *handle_clnt(void *arg);
void send_to_buzzer(void *arg);	
void send_to_led(void *arg);	
void send_to_RFID(void *arg);
void record_error(int data, int flag);
void error_handling(char *msg);
void today(struct tm *t);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
char error_log[LOG_SIZ];
int error_index[MAX_CLNT];
char present[30];
int fp[MAX_CLNT];
pthread_mutex_t mutx;
	

int main(int argc, char *argv[]) {
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int option;
	socklen_t clnt_adr_sz, optlen;
	pthread_t t_id;

	/* today's date and time */
	struct tm *t;
	time_t now = time(NULL);
	t = localtime(&now);
	/* for error_file name */
	char error_date[8], file_path[MAX_CLNT][30];
	char m[3], d[3];
	memset(&file_path, 0x00, sizeof(file_path));
	sprintf(m, "%d", t->tm_mon + 1);
	sprintf(d, "%d", t->tm_mday);
	strcpy(error_date, "[");
	strcat(error_date, m);
	strcat(error_date, d);
	strcat(error_date, "]");
	strcpy(file_path[0], "error/");
	strcat(file_path[0], error_date);
	strcat(file_path[0], "error_US.txt");
	strcpy(file_path[1], "error/");
	strcat(file_path[1], error_date);
	strcat(file_path[1], "error_CDS.txt");
		

	if(argc != 2) {
		fprintf(stderr, "Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	if((fp[0] = open(file_path[0], O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
		error_handling("write file(Ultra Sonic) error!");
	}
	if((fp[1] = open(file_path[1], O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
		error_handling("write file(CDS) error!");
	}

	memset(error_index, 0x00, sizeof(error_index));
	pthread_mutex_init(&mutx, NULL);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1) {
		error_handling("socket() error!");
	}
	optlen = sizeof(option);
	option = TRUE;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, optlen);

	memset(&serv_adr, 0x00, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
		error_handling("bind() error!");
	}
	if(listen(serv_sock, 5) == -1) {
		error_handling("listen() error!");
	}

	while(1) {
		clnt_adr_sz = sizeof(clnt_adr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
		if(clnt_sock == -1) {
			error_handling("accept() error!");
		}

		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++] = clnt_sock;
		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id);
		printf("Connected client IP : %s\n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);

	return 0;
}

void *handle_clnt(void *arg) {
	int clnt_sock = *((int*)arg);
	int str_len = 0;
	char data[BUF_SIZ];
	int int_data = 0, flag = -1;

	memset(&data, 0x00, BUF_SIZ);
	while((str_len = read(clnt_sock, data, sizeof(data) - 1)) > 0) {
		data[str_len] = '\0';	// add end char 
		// for debug
		fputs(data, stderr);

		int_data = atoi(data);
		flag = int_data % 10;
		int_data /= 10;
	        if(flag == 0) {
			if(int_data < NEAR_POINT) {
				send_to_buzzer((void*)&clnt_sock);
        			record_error(int_data, flag); 	
			}	
			else {
				write(clnt_sock, "OK", 3);
			}
		} else if(flag == 1) {
			if(int_data < DARK_POINT) {
				send_to_led((void*)&clnt_sock);	
				record_error(int_data, flag); 	
			}
			else {
				write(clnt_sock, "OK", 3);
			}
		} else if(flag == 2) {
			send_to_RFID((void*)&clnt_sock);
		}
		memset(&data, 0x00, BUF_SIZ);
	}

	pthread_mutex_lock(&mutx);
	for(int i=0;i<clnt_cnt;i++) {
		if(clnt_sock == clnt_socks[i]) {
			while(i++ < clnt_cnt - 1) {
				clnt_socks[i] = clnt_socks[i + 1];
			}
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
	close(clnt_sock);

	return NULL;
}

void send_to_buzzer(void *arg) {
	int clnt_sock = *((int*)arg);

	write(clnt_sock, "BUZ_ON", 7);
	return;
}

void send_to_led(void *arg) {
	int clnt_sock = *((int*)arg);

	write(clnt_sock, "LED_ON", 7);
	return;
}

void send_to_RFID(void *arg) {
	int clnt_sock = *((int*)arg);

	write(clnt_sock, "DETECTED", 9);
	return;
}

void record_error(int data, int flag) {
	char error_info[BUF_SIZ];
	char error_num[10], string_data[10];
	/* today's date and time */
	struct tm *t;
	time_t now = time(NULL);
	t = localtime(&now);

	sprintf(error_num, "%09d", ++error_index[flag]);
	sprintf(string_data, "%d", data);
	memset(&error_info, 0x00, BUF_SIZ);
	pthread_mutex_lock(&mutx);
	today(t);
	strcpy(error_info, error_num);
	strcat(error_info, " ");
	strcat(error_info, present);
	strcat(error_info, " Value: ");
	strcat(error_info, string_data);
	strcat(error_info, "\n");
	pthread_mutex_unlock(&mutx);

	for(int i=0;i<BUF_SIZ;i++) {
		write(fp[flag], &error_info[i], 1);
		if(!strcmp(&error_info[i], "\n")) {
			break;
		}
	}
	return;
}

void error_handling(char *msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

void today(struct tm *t) {
	char y[5], m[3], d[3], hh[3], mm[3], ss[3];

	sprintf(y, "%d", 1900 + t->tm_year);
	sprintf(m, "%02d", t->tm_mon + 1);
	sprintf(d, "%02d", t->tm_mday);
	sprintf(hh, "%02d", t->tm_hour);
	sprintf(mm, "%02d", t->tm_min);
	sprintf(ss, "%02d", t->tm_sec);

	strcpy(present, y);
	strcat(present, "/");
	strcat(present, m);
	strcat(present, "/");
	strcat(present, d);
	strcat(present, " ");
	strcat(present, hh);
	strcat(present, ":");
	strcat(present, mm);
	strcat(present, ":");
	strcat(present, ss);

	return;
}
