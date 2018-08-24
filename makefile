CC = g++
FLAGS += -g -I. -I./include -I./util -I./test 

all: client.out server.out
client.out: ./test/client_test.cpp ./Communicate/Communicate.cpp ./util/ThreadVar/ThreadVar.cpp 
	$(CC) $(FLAGS) ./test/client_test.cpp  ./Communicate/Communicate.cpp ./util/ThreadVar/ThreadVar.cpp  -lpthread -ljsoncpp -o client.out
server.out: ./test/server_test.cpp ./Server/Server.cpp ./util/ThreadVar/ThreadVar.cpp 
	$(CC) $(FLAGS) ./test/server_test.cpp  ./Server/Server.cpp ./util/ThreadVar/ThreadVar.cpp  -lpthread -ljsoncpp -o server.out

clean:
	rm -rf *.out *.o 