# ------------------------------------------------------------------------------
#     Makefile: 서버 프로그램 빌드 및 실행 자동화
#     목적: server.c를 컴파일하고 실행 파일(svr)을 생성 및 실행하는 과정 자동화
# ------------------------------------------------------------------------------




# 컴파일러 및 옵션 설정
CC = gcc						# 사용할 C 언어 컴파일러를 설정 (GNU C Compiler)

CFLAGS = -D_REENTRANT -Wall -Wextra -O2			# 컴파일 시 적용할 옵션
## -D_REENTRANT : POSIX 스레드 안전 코드 작성 시 사용
## -Wall, -Wextra : 컴파일 시 모든 경고 메시지를 활성화
## -O2 : 코드 최적화 (속도와 크기 균형)

LDFLAGS = -lpthread					# 링크 시 추가할 라이브러리
# -lpthread : POSIX 스레드를 사용하기 위한 라이브러리 링크




# 소스 파일과 목적 파일 설정
SRC = src/server.c					# 소스 파일 경로 설정
OBJ = $(SRC:.c=.o)					# 소스 파일(.c)을 목적 파일(.o)로 변환
TARGET = svr						# 최종 실행 파일의 이름




# -----------------------------------------------------------------
#    기본 규칙: all
#    'make' 명령 실행 시 기본적으로 호출되는 규칙
#    실행 파일(TARGET)을 생성
# -----------------------------------------------------------------
all: $(TARGET)




# ------------------------------------------------------------------
#    실행 파일 생성 규칙
#      - TARGET을 생성하기 위해 OBJ(목적 파일)가 필요
#      - OBJ 파일을 컴파일러를 사용해 링크하여 실행 파일 생성
# ------------------------------------------------------------------
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)
	# - $(CC): gcc 컴파일러 사용
	# - $(CFLAGS): 컴파일 시 적용할 옵션
	# - -o $(TARGET): 결과 실행 파일의 이름 설정
	# - $(OBJ): 목적 파일 입력
	# - $(LDFLAGS): 링크 시 pthread 라이브러리 추가



# ------------------------------------------------------------------
#    목적 파일 생성 규칙
#    %.o: %.c 패턴을 사용해 .c 파일을 .o 파일로 변환
#      - $< : 입력 파일 (%.c)
#      - $@ : 출력 파일 (%.o)
# ------------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	# - -c : 컴파일만 수행하고 링크는 하지 않음
	# - -o : 출력 파일 이름 설정



# ------------------------------------------------------------------
#    clean 규칙: 빌드 결과물 정리
#      - 목적 파일(.o)과 실행 파일을 삭제
#      - 'make clean' 명령어로 실행
# ------------------------------------------------------------------
clean:
	rm -f $(OBJ) $(TARGET)
	# - rm -f : 파일 삭제 명령 (-f 옵션: 존재하지 않는 파일도 무시)
	# - $(OBJ): 생성된 목적 파일 삭제
	# - $(TARGET): 실행 파일 삭제



# ------------------------------------------------------------------
#    run 규칙: 실행 파일 실행
#      - 'make run' 명령어로 실행 파일 실행
# ------------------------------------------------------------------
run: $(TARGET)
	./$(TARGET)
	# - ./$(TARGET): 실행 파일 실행


# ------------------------------------------------------------------
#    debug 규칙: 디버깅 빌드
#      - 디버깅을 위한 추가 옵션(-g)을 활성화하고 clean 후 재빌드
#      - 'make debug' 명령어로 실행
# ------------------------------------------------------------------
debug: CFLAGS += -g
debug: clean all
	# - clean: 기존 빌드 파일 정리
	# - all: 실행 파일 재생성
