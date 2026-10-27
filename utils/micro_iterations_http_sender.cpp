#include "micro_iterations_http_sender.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>

MicroIterationsHttpSender::MicroIterationsHttpSender(std::string host, int port, std::string path)
    : host_(std::move(host)), port_(port), path_(std::move(path))
{
}

MicroIterationsHttpSender::~MicroIterationsHttpSender()
{
    Disconnect();
}

bool MicroIterationsHttpSender::Connect()
{
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0)
    {
        std::cerr << "[MicroIterationsHttpSender] socket() failed: " << strerror(errno) << "\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "[MicroIterationsHttpSender] invalid address: " << host_ << "\n";
        Disconnect();
        return false;
    }

    if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "[MicroIterationsHttpSender] connect() failed: " << strerror(errno) << "\n";
        Disconnect();
        return false;
    }

    return true;
}

void MicroIterationsHttpSender::Disconnect()
{
    if (socket_fd_ >= 0)
    {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool MicroIterationsHttpSender::Send(const std::string& json_body)
{
    if (!Connect())
    {
        return false;
    }

    std::ostringstream http;
    http << "POST " << path_ << " HTTP/1.1\r\n"
         << "Host: " << host_ << ":" << port_ << "\r\n"
         << "Content-Type: application/json\r\n"
         << "Content-Length: " << json_body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << json_body;
    std::string request = http.str();

    ssize_t sent = send(socket_fd_, request.c_str(), request.size(), MSG_NOSIGNAL);
    Disconnect();

    if (sent < 0)
    {
        std::cerr << "[MicroIterationsHttpSender] send() failed: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}
