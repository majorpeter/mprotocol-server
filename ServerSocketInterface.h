/*
 * ServerSocketInterface.h
 *
 *  Created on: 2016. jan. 22.
 *      Author: peti
 */

#ifndef PROTOCOL_SERVERSOCKETINTERFACE_H_
#define PROTOCOL_SERVERSOCKETINTERFACE_H_

#include "AbstractSerialInterface.h"
#include <arpa/inet.h>

namespace std {
class thread;
}

class ServerSocketInterface: public AbstractSerialInterface {
public:
	ServerSocketInterface(uint16_t port);
	virtual ~ServerSocketInterface();
    virtual void listen();
    virtual bool writeBytes(const uint8_t* bytes, uint16_t length);
private:
    struct sockaddr_in server;
    int socket_desc;
    int client_sock;
    std::thread* serverThread;
    void serverThreadFunction();
};

#endif /* PROTOCOL_SERVERSOCKETINTERFACE_H_ */
