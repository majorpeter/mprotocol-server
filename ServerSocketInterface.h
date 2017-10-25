/*
 * ServerSocketInterface.h
 *
 *  Created on: 2016. jan. 22.
 *      Author: peti
 */

#ifndef PROTOCOL_SERVERSOCKETINTERFACE_H_
#define PROTOCOL_SERVERSOCKETINTERFACE_H_

#include "AbstractSerialInterface.h"
#ifdef LINUX
#include <arpa/inet.h>
#include <thread>
#endif


class ServerSocketInterface: public AbstractSerialInterface {
public:
    ServerSocketInterface(uint16_t port);
    virtual ~ServerSocketInterface();
    virtual void listen();
    virtual bool writeBytes(const uint8_t* bytes, uint16_t length);
    void receiveBytes(const uint8_t* bytes, uint16_t length);
private:
#ifdef LINUX
    struct sockaddr_in server;
    std::thread * serverThread;
    void serverThreadFunction();
#endif
    int socket_desc;
    int client_sock;
};

#endif /* PROTOCOL_SERVERSOCKETINTERFACE_H_ */
