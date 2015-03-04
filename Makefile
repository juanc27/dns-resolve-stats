CC := g++
CXXFLAGS := -I/usr/local/include/mysql++/ -I/usr/local/include/ -I/usr/include/mysql/
LDFLAGS := -L/usr/lib64/mysql/ -L/usr/local/lib/
LDLIBS := -lmysqlpp -lmysqlclient -lldns

all: dns_resolve_stats

server.o: server.cpp

dns_resolve_stats.o: dns_resolve_stats.cpp

dns_resolve_stats: dns_resolve_stats.o server.o 

clean:
	rm -f dns_resolve_stats *.o

