#include "RtmpServer.h"
#include "CoSocket/net/CoSocket.h"
#include "CoSocket/net/http/HttpDispatch.h"

#define RTMP_SERVER_PORT 1935
#define HTTP_SERVER_PORT 34567

void RtmpHandle(DataCache &dataCache, TcpServer::ConnectorPtr connector)
{
    std::unique_ptr<RtmpServer> rtmpServer(new RtmpServer(connector, dataCache));
    (*rtmpServer)();
}

void HttpHandle(DataCache &dataCache, const HttpDecoder &request, HttpEncoder &response)
{
    const StreamCacheMap &cache = dataCache.GetStreamCache();

    std::ostringstream body;
    body << "{";
    StreamCacheMap::const_iterator it = cache.begin();
    for (; it != cache.end(); ++it)
    {
        body << "\n"
             << "\"stream name\": \"" << it->first.c_str() << "\"\n"
             << "\"client num\": " << it->second.playerQueues.size() << "\n";
    }
    body << "}";

    response.SetStatusCode(HttpEncoder::StatusCode_200Ok);
    response.SetBody(body.str().c_str());
    return;
}

void HttpDispatchHandle(DataCache &dataCache, TcpServer::ConnectorPtr connector)
{
    HttpDispatch httpDispatch(false);
    httpDispatch.AddHandler("/", std::bind(
        &HttpHandle, std::ref(dataCache), std::placeholders::_1, std::placeholders::_2));
    httpDispatch(connector);
}

void HandleRequest(DataCache &dataCache, TcpServer::ConnectorPtr connector, unsigned short port)
{
    if (port == RTMP_SERVER_PORT)
        RtmpHandle(dataCache, connector);
    else
        HttpDispatchHandle(dataCache, connector);
}

int main()
{
    DataCache dataCache;
    TcpServer rtmpServer;

    if (!rtmpServer.Bind("0.0.0.0", RTMP_SERVER_PORT))
        return 1;

    if (!rtmpServer.Bind("0.0.0.0", HTTP_SERVER_PORT))
        return 1;

    rtmpServer.SetRequestHandler(
        std::bind(&HandleRequest, std::ref(dataCache), std::placeholders::_1, std::placeholders::_2));
    std::list<int> childIds;
    rtmpServer.ListenAndFork(1, childIds);
    rtmpServer.MonitorSlavesLoop();
    return 0;
}
