#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
using namespace std;

// #include <iomanip> // for hex, setw, etc.

// void printHexBytes(char *data, size_t n) {
//     cout << hex << setfill('0');
//     for (size_t i = 0; i < n; ++i) {
//         cout << setw(2) << static_cast<int>(data[i]) << " ";
//     }
//     cout << dec << endl; // Reset to decimal
// }

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

void send_all(int sockfd, void *buffer, size_t len)
{
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;

    if (send(sockfd, &len, sizeof(len), 0) < 0)
        error_exit("error sending message TCP");

    while (bytes_remaining)
    {
        bytes_sent = send(sockfd, buffer, len, 0);
        if (bytes_sent < 0)
            error_exit("error sending message TCP");
        bytes_remaining -= bytes_sent;
    }
}

void recv_all(int sockfd, void *buffer, size_t len)
{
    size_t bytes_received = 0;
    size_t bytes_remaining = len;

    while (bytes_remaining)
    {
        bytes_received = recv(sockfd, buffer, len, 0);
        if (bytes_received < 0)
            error_exit("error receiving bytes TCP");
        bytes_remaining -= bytes_received;
    }
}
