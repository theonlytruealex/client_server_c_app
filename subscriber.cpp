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

int send_subscription(uint8_t sub_state, topic_body &topic, int sockfd)
{
    subscription sub;
    sub.sub_state = sub_state;
    sub.len = htonl(sizeof(sub.sub_state) + sizeof(topic.size) + topic.size);
    sub.topic = topic;
    sub.topic.size = htonl(sub.topic.size);
    return send_all(sockfd, &sub, ntohl(sub.len) + sizeof(sub.len));
}

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

    // Send id
    id[strlen(id)] = '\0';
    id[10] = '\0';
    uid user_id;
    strcpy(user_id.id, id);
    user_id.size = htonl(strlen(id) + 1);

    if (send_all(tcp_socket, &user_id, strlen(id) + 1 + sizeof(user_id.size)) < 0)
        error_exit("Could not Communicate with server");

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
            cout << "Got message";
            topic_update buffer;
            if (recv_all(tcp_socket, &buffer) < 0)
                error_exit("Error receiving server topic");
            buffer.len = ntohl(buffer.len);
            buffer.payload_len = ntohl(buffer.payload_len);
            cout << buffer.preambule << buffer.topic.cells << buffer.payload << '\n';
        }
        else if (fd == STDIN_FILENO)
        {
            string input;
            getline(cin, input);
            if (input.length() < 4)
            {
                write(STDERR_FILENO, "Bad Command\n", 12);
                continue;
            }
            if (input.compare("exit") == 0)
                break;
            if (input.length() < 11)
            {
                cout << input;
                write(STDERR_FILENO, "Bad Command\n", 12);
                continue;
            }
            if (!input.substr(0, 10).compare("subscribe "))
            {
                topic_body topic;
                int spacecount = 0;

                // remove leading whitespaces
                while (input[9 + spacecount] == ' ')
                    spacecount++;
                topic.size = input.length() - 9 - spacecount;
                strncpy(topic.cells, input.c_str() + 9 + spacecount, 51);
                topic.cells[topic.size] = '\0';
                topic.size++;

                // send subscription update
                if (send_subscription(1, topic, tcp_socket) < 0)
                    error_exit("Failed to send subscription to server");

                cout << "Subscribed to topic " << topic.cells << '\n';
                continue;
            }
            if (input.length() < 13)
            {
                write(STDERR_FILENO, "Bad Command\n", 12);
                continue;
            }
            if (!input.substr(0, 12).compare("unsubscribe "))
            {
                topic_body topic;
                int spacecount = 0;

                // remove leading whitespaces
                while (input[11 + spacecount] == ' ')
                    spacecount++;
                topic.size = input.length() - 11 - spacecount;
                strncpy(topic.cells, input.c_str() + 11 + spacecount, 51);
                topic.cells[topic.size] = '\0';
                topic.size++;

                // send subscription update
                if (send_subscription(0, topic, tcp_socket) < 0)
                    error_exit("Failed to send unsubscribe command to server");

                cout << "Unsubscribed from topic " << topic.cells << '\n';
                continue;
            }
            else
            {
                write(STDERR_FILENO, "Bad Command\n", 12);
                continue;
            }
        }
    }

    close(tcp_socket);
    close(epollfd);
}
