CC = g++
FLAGS += -std=c++11 -g -I./src -I./src/include -I./src/util -I./src/test


all: client.out server.out leveldbserver.out clusterserver.out gateserver.out

#线程变量类
ThreadVar.o: ./src/util/ThreadVar/ThreadVar.cpp
	$(CC) $(FLAGS) -c ./src/util/ThreadVar/ThreadVar.cpp -o ./build/ThreadVar.o
#通信类
Communicate.o:  ./src/Communicate/Communicate.cpp 
	$(CC) $(FLAGS) -c ./src/Communicate/Communicate.cpp -o ./build/Communicate.o

#server 类
Server.o: ./src/Server/Server.cpp
	$(CC) $(FLAGS) -c ./src/Server/Server.cpp -o ./build/Server.o

#集群服务器
ClusterServer.o: ./src/ClusterServer/ClusterServer.cpp
	$(CC) $(FLAGS) -c ./src/ClusterServer/ClusterServer.cpp  -o ./build/ClusterServer.o


Syncobj.o: ./src/util/Syncobj/Syncobj.cpp 
	$(CC) $(FLAGS) -c ./src/util/Syncobj/Syncobj.cpp -o ./build/Syncobj.o 

Chord.o: ./src/util/Chord/Chord.cpp
	$(CC) $(FLAGS) -c ./src/util/Chord/Chord.cpp  -o ./build/Chord.o
LevelDbServer.o: ./src/LevelDbServer/LevelDbServer.cpp 
	$(CC) $(FLAGS) -c ./src/LevelDbServer/LevelDbServer.cpp  -o ./build/LevelDbServer.o

GateServer.o: ./src/GateServerTemp/GateServer.cpp
	$(CC) $(FLAGS) -c ./src/GateServerTemp/GateServer.cpp -o ./build/GateServer.o

client.out: ./src/test/client_test.cpp ThreadVar.o Communicate.o Server.o 
	$(CC) $(FLAGS) ./src/test/client_test.cpp ./build/ThreadVar.o ./build/Communicate.o  ./build/Server.o -lleveldb -ljsoncpp -lpthread -o ./build/client.out 

server.out: ./src/test/server_test.cpp Communicate.o Server.o ThreadVar.o 
	$(CC) $(FLAGS) ./src/test/server_test.cpp ./build/ThreadVar.o ./build/Communicate.o ./build/Server.o -o ./build/server.out -lleveldb -ljsoncpp -lpthread


clusterserver.out: ./src/test/cluster_test.cpp ./src/test/debug.cpp ClusterServer.o Communicate.o ThreadVar.o Server.o 
	$(CC) $(FLAGS) ./src/test/cluster_test.cpp ./src/test/debug.cpp ./build/ClusterServer.o ./build/Communicate.o ./build/Server.o ./build/ThreadVar.o -o ./build/clusterserver.out -lleveldb -ljsoncpp -lpthread
leveldbserver.out: ./src/test/level_db_test.cpp LevelDbServer.o Syncobj.o  Communicate.o  Server.o ThreadVar.o  Chord.o 
	$(CC) $(FLAGS) ./src/test/level_db_test.cpp ./build/Syncobj.o ./build/Communicate.o ./build/Chord.o ./build/ThreadVar.o ./build/Server.o ./build/LevelDbServer.o -o ./build/leveldbserver.out -lleveldb -ljsoncpp -lpthread


gateserver.out: ./src/test/gateserver_test.cpp ./src/test/debug.cpp Communicate.o Server.o ThreadVar.o Syncobj.o  GateServer.o  ClusterServer.o 
	$(CC) $(FLAGS) ./src/test/gateserver_test.cpp ./src/test/debug.cpp ./build/Communicate.o ./build/Server.o ./build/ClusterServer.o ./build/ThreadVar.o ./build/Syncobj.o ./build/GateServer.o -o ./build/gateserver.out -lleveldb -ljsoncpp -lpthread
clean:
	rm -rf ./build/*.out  ./build/*.o 
