#pragma once

#include <string.h>
#include <map>
#include <exception>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#include <iostream>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <sys/select.h>
// #include <time.h>
// #include <cstring>
// #include <getopt.h>

#include <unistd.h>     // close socket

#define RX_PORT         (19101)
#define RX_BUFFER_SIZE  (1024)
#define TS_BUF_SIZE     (19)

#ifdef __APPLE__
//    MSG_EOR         0x8             /* data completes record */ 
// or MSG_EOF         0x100           /* data completes connection */
#define SOCK_SND_FLAG MSG_EOR
#else
// MSG_CONFIRM
#define SOCK_SND_FLAG (0x800)
#endif

void getTimeStamp(char* pTimeStamp, int buffSize);

class SocketConnection {
public:
    SocketConnection(const int rxport);
    int GetSocketFd();
    int Bind();
    std::string Recv();
    void Send(const std::string msgString, const int msgStrSize);
private:
    int _rxPort;
    int _sockfd;
    struct sockaddr_in _serverAddr;
    struct sockaddr_in _clientAddr;
    char _buffer[RX_BUFFER_SIZE];
    char _clientAddrString[INET_ADDRSTRLEN];
};

