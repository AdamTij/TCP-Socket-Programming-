all: pa4_server pa4_client

pa4_client: pa4_client.cpp
	g++ pa4_client.cpp -o pa4_client

pa4_server: pa4_server.cpp
	g++ pa4_server.cpp -lpthread -o pa4_server

clean:
	rm -f pa4_server pa4_client
