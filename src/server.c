/*
 * Arduino 제어 서버 프로그램
 *
 * 목적:
 *  - 다중 클라이언트가 접속하여 서버로 데이터를 전송
 *  - 서버는 수신된 데이터를 분석하여 특정 조건에 따라 Buzzer, LED, Servo motor 장치를 제어
 *  - 에러 상황이 발생하면 로그 파일에 기록하여 날짜별로 저장
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
void send_to_servo(void *arg);
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
		clnt_adr_sz = sizeof(clnt_adr);							// 클라이언트 주소 구조체의 크기 설정
		/* 4. accpet() : 클라이언트의 연결 요청을 수락 */
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);	
		if(clnt_sock == -1) {								// 클라이언트 연결 수락 실패 시 에러 메세지 출력 후 프로그램 종료
			error_handling("accept() error!");
		}

		/* 안정적인 클라이언트 디스크립터의 관리를 위해 mutex를 잠가 다른 스레드의 동시 접근 방지 */
		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++] = clnt_sock;
		pthread_mutex_unlock(&mutx);

		/* 새 클라이언트 처리를 위한 스레드 생성 */
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id);								// 분리된 스레드는 스스로 종료 시 리소스를 자동으로 반환(좀비 스레드 방지)
		printf("Connected client IP : %s\n", inet_ntoa(clnt_adr.sin_addr));		// 클라이언트의 IP 주소를 출력해 연결된 클라이언트를 확인
	}
	
	/* 루프를 벗어나면 서버 소켓을 닫아 리소스 정리 */
	close(serv_sock);

	return 0;
}

void *handle_clnt(void *arg) {
	/* 
 	 * 목적:
    	 *  - 클라이언트가 보낸 데이터를 읽고 분석하여 장치 제어 명령 송신 
      	 *  - 임계 범위에 포함될 시 에러 로그 기록
	 *  - 클라이언트 연결이 종료되면 소켓을 정리하고 종료
    	 */ 
	int clnt_sock = *((int*)arg);			// 클라이언트 소켓 디스크립터
	int str_len = 0;				// 수신된 데이터 길이를 저장하는 변수
	char data[BUF_SIZ];				// 클라이언트로부터 받은 데이터를 저장할 버퍼
	int int_data = 0, flag = -1;			// 데이터 분석용 변수 (숫자 값 및 flag 값)

	memset(&data, 0x00, BUF_SIZ);			// 데이터 버퍼를 0으로 초기화

	/* 루프를 통해 클라이언트로부터 데이터를 반복해서 read */
	while((str_len = read(clnt_sock, data, sizeof(data) - 1)) > 0) {
		data[str_len] = '\0';			// 문자열의 끝에 NULL 문자('\0') 추가하여 문자열 종료

		fputs(data, stderr);			// 수신 데이터 확인을 위한 출력

		/* 수신 데이터 변환 */
		int_data = atoi(data);			// 문자열 데이터를 정수형으로 변환
		flag = int_data % 10;			// flag 값: 데이터의 마지막 숫자 (장치 구분)
		int_data /= 10;				// 나머지 숫자는 센서 데이터 값으로 사용

		/* 
	         * flag 값에 따라 장치(클라이언트)를 구분하여 처리
	         *  - flag == 0: 초음파 센서 → Buzzer 제어
	         *  - flag == 1: CDS 센서 → LED 제어
	         *  - flag == 2: RFID 센서 → Servo motor 제어
	         */
	        if(flag == 0) {		// Buzzer 제어 조건
			if(int_data < NEAR_POINT) {			// 초음파 센서 값이 NEAR_POINT 이하인 경우
				send_to_buzzer((void*)&clnt_sock);	// 클라이언트에 Buzzer 제어 지시를 위한 메세지 전송
        			record_error(int_data, flag); 		// 에러 로그 기록
			}	
			else {				
				if(write(clnt_sock, "OK", 3) < 0) {	// 정상 상태 메세지 전송
					error_handling("write \"ok\" in handle_clnt error!");
				}		
			}
		} else if(flag == 1) {	// LED 제어 조건
			if(int_data < DARK_POINT) {			// CDS 센서 값이 DARK_POINT 이하인 경우
				send_to_led((void*)&clnt_sock);		// 클라이언트에 LED 제어 지시를 위한 메세지 전송
				record_error(int_data, flag); 		// 에러 로그 기록
			}
			else {									
				if(write(clnt_sock, "OK", 3) < 0) {	// 정상 상태 메세지 전송
					error_handling("write \"ok\" in handle_clnt error!");
				}						}
		} else if(flag == 2) {	// Servo motor 제어 조건
			send_to_servo((void*)&clnt_sock);		// 클라이언트에 Servo motor 제어 지시를 위한 메세지 전송
		}
		
		memset(&data, 0x00, BUF_SIZ);				// 데이터 버퍼를 다시 0으로 초기화
	}

	/* 클라이언트 연결이 종료되었을 때 클라이언트 디스크립터 배열에서 해당 데이터를 삭제 */
	pthread_mutex_lock(&mutx);					// 공유 자원이므로 mutex 잠금
	for(int i=0;i<clnt_cnt;i++) {
		if(clnt_sock == clnt_socks[i]) {			// 현재 클라이언트 소켓을 찾음
			while(i++ < clnt_cnt - 1) {			// 배열에서 해당 클라이언트를 제거
				clnt_socks[i] = clnt_socks[i + 1];
			}
			break;
		}
	}
	clnt_cnt--;							// 클라이언트 수를 감소시킴
	pthread_mutex_unlock(&mutx);					// 뮤텍스 잠금 해제

	/* 클라이언트 소켓을 닫아 연결 종료 */
	close(clnt_sock);

	return NULL;		// 스레드 종료
}

void send_to_buzzer(void *arg) {
	/*
	 * 목적: 
	 *  - buzzer 측 클라이언트에 buzzer 제어 메세지 송신
	 */
	int clnt_sock = *((int*)arg);			// 클라이언트 소켓 디스크립터

	if(write(clnt_sock, "BUZ_ON", 7) < 0) {		// 클라이언트 소켓으로 "BUZ_ON" 문자열을 송신
		error_handling("write \"BUZ_ON\" in send_to_buzzer()");
	}
	return;						// 함수 종료
}

void send_to_led(void *arg) {
	/*
	 * 목적: 
	 *  - led 측 클라이언트에 led 제어 메세지 송신
	 */
	int clnt_sock = *((int*)arg);			// 클라이언트 소켓 디스크립터

	if(write(clnt_sock, "LED_ON", 7) < 0) {		// 클라이언트 소켓으로 "LED_ON" 문자열을 송신
		error_handling("write \"LED_ON\" in send_to_led()");
	}
	return;						// 함수 종료
}

void send_to_servo(void *arg) {
	/*
	 * 목적: 
	 *  - servo motor 측 클라이언트에 servo motor 제어 메세지 송신
	 */
	int clnt_sock = *((int*)arg);			// 클라이언트 소켓 디스크립터

	if(write(clnt_sock, "Z", 9) < 0) {		// 클라이언트 소켓으로 "DETECTED" 문자열을 송신
		error_handling("write \"Z\" in send_to_servo()");
	}
	return;						// 함수 종료
}

void record_error(int data, int flag) {
	/*
	 * 목적: 
	 *  - 센서 값이 임계 범위에 포함되었을 시 해당 기록을 에러 로그 파일에 저장
	 */
	char error_info[BUF_SIZ];				// 에러 정보를 저장할 버퍼 (번호/날짜와 시간/수신값)
	char error_num[10], string_data[10];			// 에러 번호 및 센서 데이터를 문자열로 변환할 버퍼
	
	/* 현재 날짜와 시간 가져오기 */
	struct tm *t;						// 시간 정보를 저장할 구조체 포인터
	time_t now = time(NULL);				// 현재 시간을 초 단위로 저장
	t = localtime(&now);					// 초 단위 시간을 년/월/일/시/분/초 형태로 변환

	/*
         * 에러 번호와 센서 데이터를 문자열로 변환
         *  - error_num: 9자리로 설정한 에러 발생 횟수
         *  - string_data: 문자열로 변환한 센서 데이터
         */
	sprintf(error_num, "%09d", ++error_index[flag]);	// 에러 번호 (9자리)
	sprintf(string_data, "%d", data);			// 센서 데이터를 문자열로 변환
	memset(&error_info, 0x00, BUF_SIZ);			// 에러 정보 버퍼 초기화를 통해 쓰레기 값 제거

	pthread_mutex_lock(&mutx);				// 에러 로그 내용(특히 present)은 공유 자원이므로 mutex 잠금
	today(t);						// 현재 날짜와 시간을 present 변수에 저장

	/*
         * 에러 로그 문자열 생성
         *  - 에러 번호 → 시간 정보 → 센서 값 → 줄 바꿈 순서로 조합
         */
	strcpy(error_info, error_num);				// 에러 번호를 추가
	strcat(error_info, " ");
	strcat(error_info, present);				// 날짜와 시간 추가
	strcat(error_info, " Value: ");	
	strcat(error_info, string_data);			// 센서 값 추가
	strcat(error_info, "\n");
	pthread_mutex_unlock(&mutx);				// 뮤텍스 잠금 해제

	/*
         * 에러 로그를 한 글자씩 파일에 기록
         *  - write()를 사용해 파일 포인터(fp[flag])에 쓰기
         *  - '\n' (줄 바꿈)을 만나면 기록 종료
         */
	for(int i=0;i<BUF_SIZ;i++) {
		if(write(fp[flag], &error_info[i], 1) < 0) {		// 한 글자씩 파일에 기록
			error_handling("write error information in record_error()");
		}
		if(!strcmp(&error_info[i], "\n")) {			// 줄 바꿈 문자를 만나면 종료
			break;
		}
	}
	return;								// 함수 종료
}

void error_handling(char *msg) {
	/*
	 * 목적: 
	 *  - 에러 발생 시 해당 에러 메세지를 출력하고 종료
	 */
	fputs(msg, stderr);		// 에러 메시지를 표준 에러 출력(stderr)에 출력
	fputc('\n', stderr);
	exit(1);			// 프로그램 종료 (비정상 종료)
}

void today(struct tm *t) {
	/*
	 * 목적: 
	 *  - 초 단위로 저장한 현재 날짜와 시간을 문자열 형태로 변환하여 present 변수에 저장
	 */
	char y[5], m[3], d[3], hh[3], mm[3], ss[3];

	sprintf(y, "%d", 1900 + t->tm_year);
	// 각 데이터를 두 자리 숫자로 저장
	sprintf(m, "%02d", t->tm_mon + 1);
	sprintf(d, "%02d", t->tm_mday);
	sprintf(hh, "%02d", t->tm_hour);
	sprintf(mm, "%02d", t->tm_min);
	sprintf(ss, "%02d", t->tm_sec);

	/* 문자열 조합을 통해 "yyyy/mm/dd hh:mm:ss" 형식으로 날짜와 시간 저장 */
	strcpy(present, y);
	strcat(present, "/");	// 구분자
	strcat(present, m);
	strcat(present, "/");	// 구분자
	strcat(present, d);
	strcat(present, " ");	// 구분자
	strcat(present, hh);
	strcat(present, ":");	// 구분자
	strcat(present, mm);
	strcat(present, ":");	// 구분자
	strcat(present, ss);

	return;			// 함수 종료
}
