#include <string.h>
#include "comm.h"

SocketConnection::SocketConnection(const int rxport) {
    _rxPort = rxport;
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0); // <---- err

    // // Set non-blocking mode
    // int flags = fcntl(sockfd, F_GETFL, 0);
    // fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    memset(&_serverAddr, 0, sizeof(_serverAddr));
    memset(&_clientAddr, 0, sizeof(_clientAddr));

    _serverAddr.sin_family = AF_INET; // IP v4
    _serverAddr.sin_addr.s_addr = INADDR_ANY;
    _serverAddr.sin_port = htons(RX_PORT);
    int bindResult = Bind();
    if (bindResult < 0) {
        perror("UDP port binding failed");
        exit(EXIT_FAILURE);
    }
}

int SocketConnection::Bind() {
    int result = bind(
        _sockfd, 
        (const struct sockaddr *)&_serverAddr, 
        sizeof(_serverAddr)
    ); // <--- err, close sock
    
    return result;
}

int SocketConnection::GetSocketFd() {
    return _sockfd;
}

std::string SocketConnection::Recv() {
    unsigned int len, n;
    std::string result = "";
    len = sizeof(_clientAddr);

    n = recvfrom(
        _sockfd, 
        (char *)_buffer, 
        RX_BUFFER_SIZE,
        MSG_WAITALL, 
        (struct sockaddr *) &_clientAddr, 
        &len
    );

    if (n >= RX_BUFFER_SIZE) n = RX_BUFFER_SIZE - 1; // <--- err if <0, close sock
    _buffer[n] = '\0';

    char time_stmp[] = "0000-00-00 00:00:00";
    getTimeStamp(time_stmp, sizeof(time_stmp));
    printf("%s Received request: %s [%s:%0d]\n", 
        time_stmp, 
        _buffer, 
        inet_ntop(
            AF_INET, 
            &_clientAddr.sin_addr.s_addr, 
            _clientAddrString, 
            sizeof(_clientAddrString)
        ), 
        ntohs(_clientAddr.sin_port)
    );

    result = _buffer;
    return result;
}

void SocketConnection::Send(const std::string msgString, const int msgStrSize) {
    char time_stmp[] = "0000-00-00 00:00:00";
    getTimeStamp(time_stmp, sizeof(time_stmp));

    sendto(
        _sockfd, 
        &msgString[0], 
        msgStrSize, 
        /*MSG_CONFIRM*/SOCK_SND_FLAG, 
        (const struct sockaddr *) &_clientAddr, 
        sizeof(_clientAddr)
    ); // <-- err, close sock
    
    printf("%s Response sent: %s [%s:%0d]\n", 
        time_stmp, 
        msgString.c_str(),
        inet_ntop(AF_INET, &_clientAddr.sin_addr.s_addr, _clientAddrString, sizeof(_clientAddrString)), 
        ntohs(_clientAddr.sin_port)
    );
}

void getTimeStamp(char* pTimeStamp, int buffSize) {
    time_t currentTime;
    struct tm* tm;
    currentTime = time(NULL);
    tm = localtime(&currentTime);
    
    if (buffSize >= TS_BUF_SIZE) { 
        snprintf(pTimeStamp, buffSize, "%04d-%02d-%02d %02d:%02d:%02d", 
            tm->tm_year + 1900, 
            tm->tm_mon + 1, 
            tm->tm_mday, 
            tm->tm_hour, 
            tm->tm_min, 
            tm->tm_sec
        );
    }
}
