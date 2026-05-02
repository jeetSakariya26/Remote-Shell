all : server client

server : server.out helper.out packet.out
	g++ -std=c++17 -o server server.cpp packet.cpp helper.cpp -lutil

client : client.out helper.out packet.out
	g++ -std=c++17 -o client client.cpp packet.cpp helper.cpp -lutil

server.out : server.cpp packet.h helper.h
	g++ -std=c++17 -c server.cpp -o server.out

client.out : client.cpp packet.h helper.h
	g++ -std=c++17 -c client.cpp -o client.out

helper.out : helper.cpp helper.h
	g++ -std=c++17 -c helper.cpp -o helper.out

packet.out : packet.cpp packet.h
	g++ -std=c++17 -c packet.cpp -o packet.out

clean :
	rm -f server.out client.out helper.out packet.out server client