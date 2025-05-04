#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
using namespace std;

void error_exit(const std::string &s)
{
    cout << s << '\n';
    exit(1);
}

int send_all(int sockfd, void *buffer, uint32_t len)
{
    uint32_t bytes_sent = 0;
    uint32_t bytes_remaining = len;
    char *char_buffer = (char *)buffer;
    while (bytes_remaining)
    {
        char_buffer += bytes_sent;
        bytes_sent = send(sockfd, char_buffer, bytes_remaining, 0);
        if (bytes_sent < 0)
            return -1;
        bytes_remaining -= bytes_sent;
    }
    return len;
}

int recv_all(int sockfd, void *buffer)
{
    uint32_t bytes_received = 0;
    uint32_t bytes_remaining = sizeof(uint32_t);
    char *char_buffer = (char *)buffer;

    while (bytes_remaining)
    {
        bytes_received = recv(sockfd, char_buffer, bytes_remaining, 0);
        if (bytes_received < 0)
            return -1;
        else if (bytes_received == 0)
            return 0;
        bytes_remaining -= bytes_received;
        char_buffer += bytes_received;
    }

    uint32_t *len = (uint32_t*)(char_buffer - sizeof(uint32_t));
    bytes_received = 0;
    bytes_remaining = ntohl(len[0]);

    while (bytes_remaining)
    {
        bytes_received = recv(sockfd, char_buffer, bytes_remaining, 0);
        if (bytes_received < 0)
            return -1;
        bytes_remaining -= bytes_received;
        char_buffer += bytes_received;
    }
    return 1;
}

int add_to_epoll(int epollfd, int sockfd) {
    struct epoll_event ev_tcp;
    ev_tcp.events = EPOLLIN;
    ev_tcp.data.fd = sockfd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev_tcp);
}

