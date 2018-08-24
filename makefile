CC = g++
FLAGS += -g -I./src -I./src/include -I./src/util -I./src/test 

all: client.out server.out
client.out: ./src/test/client_test.cpp ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp 
	$(CC) $(FLAGS) ./src/test/client_test.cpp  ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp  -lpthread -ljsoncpp -o ./build/client.out
server.out: ./src/test/server_test.cpp ./src/Server/Server.cpp ./src/util/ThreadVar/ThreadVar.cpp 
	$(CC) $(FLAGS) ./src/test/server_test.cpp  ./src/Server/Server.cpp ./src/util/ThreadVar/ThreadVar.cpp  -lpthread -ljsoncpp -o ./build/server.out

clean:
	rm -rf ./build/*.out  ./build/*.o 