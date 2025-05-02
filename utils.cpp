#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
using namespace std;

#include <iomanip> // for hex, setw, etc.
#include "utils.h"
#include <netinet/in.h>

void printHexBytes(char *data, uint32_t n)
{
    cout << hex << setfill('0');
    for (uint32_t i = 0; i < n; ++i)
    {
        cout << setw(2) << static_cast<int>(data[i]) << " ";
    }
    cout << dec << endl; // Reset to decimal
}

void error_exit(const std::string &s)
{
    cout << s << '\n';
    exit(1);
}

int epoll_add(int epollfd, int fd, void *ptr, int event)
{
    struct epoll_event ev;

    ev.events = event;
    ev.data.ptr = ptr;

    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int send_all(int sockfd, void *buffer, uint32_t len)
{
    uint32_t bytes_sent = 0;
    uint32_t bytes_remaining = len;
    while (bytes_remaining)
    {
        buffer += bytes_sent;
        bytes_sent = send(sockfd, buffer, bytes_remaining, 0);
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

    while (bytes_remaining)
    {
        bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
        if (bytes_received < 0)
            return -1;
        else if (bytes_received == 0)
            return 0;
        bytes_remaining -= bytes_received;
        buffer += bytes_received;
    }

    uint32_t *len = (uint32_t*)(buffer - sizeof(uint32_t));
    bytes_received = 0;
    bytes_remaining = ntohl(len[0]);

    while (bytes_remaining)
    {
        bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
        if (bytes_received < 0)
            return -1;
        bytes_remaining -= bytes_received;
        buffer += bytes_received;
    }
    return 1;
}

