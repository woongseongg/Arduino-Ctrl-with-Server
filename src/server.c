/*
 * Arduino 제어 서버 프로그램
 *
 * 목적:
 * - 다중 클라이언트가 접속하여 서버로 데이터를 전송
 * - 서버는 수신된 데이터를 분석하여 특정 조건에 따라 Buzzer, LED, Servo motor 장치를 제어
 * - 에러 상황이 발생하면 로그 파일에 기록하여 날짜별로 저장
 *
 * 주요 기능:
 * 1. 다중 클라이언트 지원: pthread(POSIX Thread)를 통해 동시에 여러 클라이언트 처리
 * 2. 데이터 분석 및 제어: 클라이언트로부터 수신된 데이터를 기준으로 장치 제어를 위한 메세지 송신
 * 3. 에러 기록: 특정 상황 발생 시 error/ 디렉터리 위치에 로그 파일을 생성하여 날짜/시간/수신 값을 기록
 *
 * 동작 흐름:
 * 1. 서버는 지정된 포트에서 클라이언트의 연결을 대기
 * 2. 클라이언트가 연결되면 데이터를 수신하고 분석
 *    - 데이터 값과 flag 값에 따라 Buzzer, LED, Servo motor 제어 명령을 송신
 *    - 센서 데이터 값이 임계 범위에 포함될 때 에러 로그를 기록
 * 3. 여러 클라이언트가 동시에 연결될 수 있도록 스레드를 사용하여 할당
 * 4. 프로그램은 종료될 때까지 클라이언트 연결을 지속적으로 처리
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

/* 상수 정의 */
#define TRUE 1
#define FALSE 0
#define BUF_SIZ 100		// 클라이언트와의 통신에서 사용할 버퍼의 크기
#define MAX_CLNT 10 		// 동시에 연결 가능한 최대 클라이언트 수
#define LOG_SIZ 30		// 에러 로그를 저장할 수 있는 최대 크기
#define NEAR_POINT 10		// Buzzer 동작 기준이 되는 초음파 센서 값 (임계값)
#define DARK_POINT 400		// LED를 켜는 기준이 되는 CDS 센서 값 (임계값)

/* 함수 원형 선언 */
void *handle_clnt(void *arg);
void send_to_buzzer(void *arg);	
void send_to_led(void *arg);	
void send_to_RFID(void *arg);
void record_error(int data, int flag);
void error_handling(char *msg);
void today(struct tm *t);

/* 전역 변수 */
int clnt_cnt = 0;		 // 현재 접속한 클라이언트 수를 저장하는 변수
int clnt_socks[MAX_CLNT];	 // 최대 MAX_CLNT 수 만큼의 클라이언트 소켓을 저장하는 배열
char error_log[LOG_SIZ];	 // 에러 메시지를 저장하기 위한 버퍼
int error_index[MAX_CLNT];	 // 각 클라이언트에 대한 에러 발생 횟수를 기록하는 배열
char present[30];		 // 현재 날짜와 시간을 저장하는 문자열 변수
int fp[MAX_CLNT];		 // 에러 로그 파일을 관리하는 파일 포인터 배열
pthread_mutex_t mutx;		 // 멀티스레드 환경에서 공유 자원을 보호하기 위한 뮤텍스
	

int main(int argc, char *argv[]) {
	int serv_sock;				// 서버 소켓 (클라이언트 연결 요청을 수락하기 위한 소켓)
	int clnt_sock;				// 클라이언트 소켓 (클라이언트와 통신하기 위한 소켓)
	struct sockaddr_in serv_adr, clnt_adr;	// 서버와 클라이언트의 주소 정보를 저장하는 구조체
	int option;				// 소켓 옵션 변수
	socklen_t clnt_adr_sz, optlen;		// 클라이언트 주소 길이 및 소켓 옵션 길이
	pthread_t t_id;				// 스레드 ID를 저장할 변수

	
	// 현재 날짜와 시간을 가져오기 위한 설정
	struct tm *t;
	time_t now = time(NULL);
	t = localtime(&now);

	/* 에러 로그 파일 이름 설정 */
	char error_date[8], file_path[MAX_CLNT][30];	// 에러 발생일 및 파일 저장 경로
	char m[3], d[3];				// 월과 일을 저장하는 문자열
	
	memset(&file_path, 0x00, sizeof(file_path));	// 파일 경로 변수 초기화
	// 현재 날짜를 문자열로 변환하여 에러 로그 파일 이름에 추가
	sprintf(m, "%02d", t->tm_mon + 1);		// 월을 문자열로 변환	
	sprintf(d, "%02d", t->tm_mday);			// 일을 문자열로 변환
	strcpy(error_date, "[");
	strcat(error_date, m);				// 월 추가
	strcat(error_date, d);				// 일 추가
	strcat(error_date, "]");
	
	// 초음파 센서 로그 파일 이름 및 경로
	strcpy(file_path[0], "error/");			
	strcat(file_path[0], error_date);
	strcat(file_path[0], "error_US.txt");
	// CDS 센서 로그 파일 이름 및 경로
	strcpy(file_path[1], "error/");
	strcat(file_path[1], error_date);
	strcat(file_path[1], "error_CDS.txt");

	
	/* 명령줄 인수가 부족한 경우 오류 출력 및 종료 */
	if(argc != 2) {
		fprintf(stderr, "Usage : %s <port>\n", argv[0]);	// 사용법 안내 메세지 출력
		exit(1);						// 프로그램 종료
	}

	/* 에러 로그 파일 열기(쓰기 모드) */
	if((fp[0] = open(file_path[0], O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
		error_handling("write file(Ultra Sonic) error!");
	}
	if((fp[1] = open(file_path[1], O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
		error_handling("write file(CDS) error!");
	}
	memset(error_index, 0x00, sizeof(error_index));
	
	/* 클라이언트 배열 접근을 동기화하기 위한 뮤텍스 초기화 */
	pthread_mutex_init(&mutx, NULL);

	/* 1. socket() : 서버 소켓 생성 (TCP, IPv4) */
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1) {					// 소켓 생성 실패 시 오류 처리
		error_handling("socket() error!");
	}

	/* 소켓 옵션 설정 (주소 재사용을 허용하여 포트 재할당 시 지속적인 bind 에러를 방지) */
	optlen = sizeof(option);
	option = TRUE;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, optlen);

	/* 서버 주소 구조체 초기화 */
	memset(&serv_adr, 0x00, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;				// IPv4 프로토콜
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);		// 모든 IP 주소에서 연결 허용
	serv_adr.sin_port = htons(atoi(argv[1]));		// 포트 번호 설정 (명령행 인수에서 가져옴)

	/* 2. bind() : 서버 소켓과 주소를 바인딩 */
	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
		error_handling("bind() error!");
	}
	/* 3. listen() : 클라이언트 연결 대기 (대기 큐 크기 5) */
	if(listen(serv_sock, 5) == -1) {
		error_handling("listen() error!");
	}

	/* 루프를 통해 클라이언트 연결 요청 처리 */
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
