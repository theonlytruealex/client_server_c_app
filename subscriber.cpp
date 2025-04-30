#include <iostream>
#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 4)
        error_exit("Bad arguments");

    char *id = argv[1];
    int port = atoi(argv[3]);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    struct sockaddr_in servaddr;
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
        error_exit("TCP socket creation failed");
    // Initialise address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    if (inet_aton(argv[2], &servaddr.sin_addr) < 0)
    {
        error_exit("Bad ip address");
    }
    servaddr.sin_port = htons(port);

    // Since Linux 2.6.8, the size argument is ignored, but must be greater
    // than zero
    int epollfd = epoll_create(100);
    if (epollfd < 0)
        error_exit("epoll instance failed to be created");

    // TCP_NODELAY activate to stop Nagle algorithm
    int flag = 1;
    if (setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
        error_exit("setsockopt TCP_NODELAY failed");

    // Add sockets and STDIN to epoll
    if (connect(tcp_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        error_exit("connection not realised");

    send_all(tcp_socket, id, strlen(id));

    struct epoll_event ev_tcp;
    ev_tcp.events = EPOLLIN;
    ev_tcp.data.fd = tcp_socket;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tcp_socket, &ev_tcp);

    struct epoll_event command_line;
    command_line.events = EPOLLIN;
    command_line.data.fd = STDIN_FILENO;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &command_line);

    while (true)
    {
        struct epoll_event event;
        int n = epoll_wait(epollfd, &event, 1, -1);
        if (n < 0)
            error_exit("epoll_wait");

        int fd = event.data.fd;

        if (fd == tcp_socket)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
        }
        else if (fd == STDIN_FILENO)
        {
            string input;
            cin >> input;
            if (input.compare("exit") == 0)
                break;
            else if (input.substr(0, 10).compare("subscribe ")) {
                continue;
            } else if (input.substr(0, 12).compare("unsubscribe ")) {
                continue;
            } else {
                write(STDERR_FILENO, "Bad Command", 11);
                continue;
            }
        }
    }

    close(tcp_socket);
    close(epollfd);
}
