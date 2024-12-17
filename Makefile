CC = gcc
CFLAGS = -D_REENTRANT -Wall -Wextra -O2
LDFLAGS = -lpthread


SRC = src/server.c
OBJ = $(SRC:.c=.o)
TARGET = svr


all: $(TARGET)



$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)



%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@



clean:
	rm -f $(OBJ) $(TARGET)



run: $(TARGET)
	./$(TARGET)



debug: CFLAGS += -g
debug: clean all
