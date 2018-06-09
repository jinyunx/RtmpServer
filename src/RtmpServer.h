#ifndef RTMP_SERVER_H
#define RTMP_SERVER_H

#include "DataCache.h"
#include "TcpServer.h"
#include "libhttp/HttpDispatch.h"

class RtmpServer
{
public:
    RtmpServer(boost::asio::io_service &service);

    void Start();

private:
    void HandleStatus(const HttpRequester &request,
                      HttpResponser &response);

    DataCache m_dataCache;
    //HttpDispatch m_httpDispatch;
    TcpServer m_tcpServer;
};

#endif // RTMP_SERVER_H
