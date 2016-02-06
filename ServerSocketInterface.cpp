/*
 * ServerSocketInterface.cpp
 *
 *  Created on: 2016. jan. 22.
 *      Author: peti
 */

#include "ServerSocketInterface.h"
#include "AbstractUpLayer.h"
#include "ProtocolParser.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#ifdef LINUX
#include <sys/socket.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


#define DEFAULT_BUFLEN 2000

static ServerSocketInterface* instance = NULL;

static SOCKET ListenSocket = INVALID_SOCKET;
static SOCKET ClientSocket = INVALID_SOCKET;

static void threadFunc(void*) {
	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	int iResult = listen(ListenSocket, 1);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	 do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);


			instance->receiveBytes((uint8_t*) recvbuf, iResult);

		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else  {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();
}
#endif

ServerSocketInterface::ServerSocketInterface(uint16_t port) {
	instance = this;
#ifdef LINUX
	this->serverThread = nullptr;
	this->client_sock = 0;

	//Create socket
	this->socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1) {
		printf("Could not create socket");
		return;
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	//Try to reuse the socket
	int yes = 1;
	if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("setsockopt");
	}

	//Bind
	if (bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
		//print the error message
		perror("bind failed. Error");
		return;
	}
	puts("bind done");
#else
	WSADATA wsaData;
	int iResult;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	char portStr[6];
	sprintf(portStr, "%d", port);
	iResult = getaddrinfo(NULL, portStr, &hints, &result);
	if ( iResult != 0 ) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);

	_beginthread(threadFunc, 0, NULL);

#endif
}

ServerSocketInterface::~ServerSocketInterface() {
#ifdef LINUX
	if (this->serverThread != nullptr) {
		this->serverThread->join();
	}
#endif
}

void ServerSocketInterface::listen() {
#ifdef LINUX
	//Listen
	::listen(socket_desc, 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");

	this->serverThread = new std::thread([=]() {
		this->serverThreadFunction();
	});
#endif
}

#ifdef LINUX
void ServerSocketInterface::serverThreadFunction() {
	struct sockaddr_in client;
	int c, read_size;
	static const int buffer_size = 2000;
	uint8_t client_message[buffer_size];

	c = sizeof(struct sockaddr_in);

	//accept connections from incoming clients
	while (1) {
		client_sock = accept(socket_desc, (struct sockaddr *) &client,
				(socklen_t*) &c);
		if (client_sock < 0) {
			perror("accept failed");
			//TODO return 1;
		}
		puts("Connection accepted");

		//Receive a message from client
		while ((read_size = recv(client_sock, client_message, buffer_size, 0)) > 0) {
			if (this->uplayer != nullptr) {
				this->uplayer->receiveBytes(client_message, read_size);
			}
		}

		if (read_size == 0) {
			puts("Client disconnected");
			fflush(stdout);
		} else if (read_size == -1) {
			perror("recv failed");
		}
	}
}
#endif

bool ServerSocketInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
#ifdef LINUX
	return write(client_sock, bytes, length) == length;
#else
	return send( ClientSocket, (const char*) bytes, length, 0 ) == length;
#endif
}

void ServerSocketInterface::receiveBytes(const uint8_t* bytes, uint16_t length) {
	if (this->uplayer != nullptr) {
		this->uplayer->receiveBytes(bytes, length);
	}
}
