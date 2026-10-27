#pragma once

#include <string>

// Minimal HTTP client (raw POSIX sockets) posting a JSON body to http://host:port/<path>.
// One connection per request, same approach as IterationDataHttpSender in antares-xpansion.
class MicroIterationsHttpSender
{
public:
    MicroIterationsHttpSender(std::string host, int port, std::string path);
    ~MicroIterationsHttpSender();

    MicroIterationsHttpSender(const MicroIterationsHttpSender&) = delete;
    MicroIterationsHttpSender& operator=(const MicroIterationsHttpSender&) = delete;

    bool Send(const std::string& json_body);

private:
    bool Connect();
    void Disconnect();

    std::string host_;
    int port_;
    std::string path_;
    int socket_fd_ = -1;
};
