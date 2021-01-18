LDFLAGS=-L /usr/lib64/mysql
BIN=../bin/ais_svr
CC=g++

.PHONY:$(BIN)
$(BIN):ais_system.cpp
	$(CC) $^ -o $@ $(LDFLAGS) -lmysqlclient -std=c++11 -lpthread -ljsoncpp -lcrypto

.PHONY:clean
clean:
	rm -rf $(BIN)
