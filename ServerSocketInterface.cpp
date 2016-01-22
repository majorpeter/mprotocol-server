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
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

ServerSocketInterface::ServerSocketInterface(uint16_t port) {
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

	//Bind
	if (bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
		//print the error message
		perror("bind failed. Error");
		return;
	}
	puts("bind done");
}

ServerSocketInterface::~ServerSocketInterface() {
	if (this->serverThread != nullptr) {
		this->serverThread->join();
	}
}

void ServerSocketInterface::listen() {

	//Listen
	::listen(socket_desc, 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");

	this->serverThread = new std::thread([=]() {
		this->serverThreadFunction();
	});
}
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

bool ServerSocketInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
	return write(client_sock, bytes, length) == length;
}
