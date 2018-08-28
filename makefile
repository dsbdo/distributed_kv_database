CC = g++
FLAGS += -std=c++11 -g -I./src -I./src/include -I./src/util -I./src/test


all: client.out server.out leveldbserver.out cluster_server.out

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

GateServer.o: ./src/GateServerTemp/GateServerTemp.cpp
	$(CC) $(FLAGS) -c ./src/GateServerTemp/GateServerTemp.cpp -o ./build/GateServer.o
Syncobj.o: ./src/util/Syncobj/Syncobj.cpp 
	$(CC) $(FLAGS) -c ./src/util/Syncobj/Syncobj.cpp -o ./build/Syncobj.o 

Chord.o: ./src/util/Chord/Chord.cpp
	$(CC) $(FLAGS) -c ./src/util/Chord/Chord.cpp  -o ./build/Chord.o
LevelDbServer.o: ./src/LevelDbServer/LevelDbServer.cpp 
	$(CC) $(FLAGS) -c ./src/LevelDbServer/LevelDbServer.cpp  -o ./build/LevelDbServer.o


client.out: ./src/test/client_test.cpp ThreadVar.o Communicate.o Server.o 
	$(CC) $(FLAGS) ./src/test/client_test.cpp ./build/ThreadVar.o ./build/Communicate.o  ./build/Server.o -lleveldb -ljsoncpp -lpthread -o ./build/client.out 

server.out: ./src/test/server_test.cpp Communicate.o Server.o ThreadVar.o 
	$(CC) $(FLAGS) ./src/test/server_test.cpp ./build/ThreadVar.o ./build/Communicate.o ./build/Server.o -o ./build/server.out -lleveldb -ljsoncpp -lpthread


cluster_server.out: ./src/test/cluster_test.cpp ./src/test/debug.cpp ClusterServer.o Communicate.o ThreadVar.o Server.o 
	$(CC) $(FLAGS) ./src/test/cluster_test.cpp ./src/test/debug.cpp ./build/ClusterServer.o ./build/Communicate.o ./build/Server.o ./build/ThreadVar.o -o ./build/cluster_server.out -lleveldb -ljsoncpp -lpthread
leveldbserver.out: ./src/test/level_db_test.cpp LevelDbServer.o Syncobj.o  Communicate.o  Server.o ThreadVar.o  Chord.o 
	$(CC) $(FLAGS) ./src/test/level_db_test.cpp ./build/Syncobj.o ./build/Communicate.o ./build/Chord.o ./build/ThreadVar.o ./build/Server.o ./build/LevelDbServer.o -o leveldbserver.out -lleveldb -ljsoncpp -lpthread
# client.out: ./src/test/client_test.cpp ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp  ./src/Server/Server.cpp 
# 	$(CC) $(FLAGS) ./src/test/client_test.cpp  ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp ./src/Server/Server.cpp  -lpthread -ljsoncpp -o ./build/client.out
# server.out: ./src/test/server_test.cpp ./src/Server/Server.cpp ./src/util/ThreadVar/ThreadVar.cpp 
# 	$(CC) $(FLAGS) ./src/test/server_test.cpp  ./src/Server/Server.cpp ./src/util/ThreadVar/ThreadVar.cpp  -lpthread -ljsoncpp -o ./build/server.out
# leveldbserver.out: ./src/test/level_db_test.cpp ./src/Server/Server.cpp ./src/LevelDbServer/LevelDbServer.cpp ./src/util/Syncobj/Syncobj.cpp ./src/util/Chord/Chord.cpp ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp
# 	$(CC) $(FLAGS) ./src/test/level_db_test.cpp	./src/Server/Server.cpp ./src/LevelDbServer/LevelDbServer.cpp ./src/util/Syncobj/Syncobj.cpp ./src/util/Chord/Chord.cpp ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp -lleveldb -lpthread -ljsoncpp -o ./build/leveldbserver.out

# clusterserver.out: ./src/test/cluster_test.cpp ./src/ClusterServer/ClusterServer.cpp ./src/Server/Server.cpp  ./src/Communicate/Communicate.cpp ./src/util/ThreadVar/ThreadVar.cpp ./src/test/debug.cpp
# 	$(CC) $(FLAGS) ./src/test/cluster_test.cpp ./src/Server/Server.cpp  ./src/ClusterServer/ClusterServer.cpp ./src/Communicate/Communicate.cpp  ./src/util/ThreadVar/ThreadVar.cpp ./src/test/debug.cpp -lpthread -ljsoncpp -o ./build/cluster_server.out
clean:
	rm -rf ./build/*.out  ./build/*.o 
