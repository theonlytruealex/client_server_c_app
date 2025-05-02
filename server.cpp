#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "utils.h"
#include <math.h>

using namespace std;

int parse_payload(char *payload, uint8_t ptype, topic_update *message)
{
    char buffer[2000];

    switch (ptype)
    {
    case 0:
    {
        unsigned int value = ntohl(*((unsigned int *)(payload + 1)));
        if (payload[0] != 0)
        {

            message->payload[message->payload_len] = '-';
        }
        return snprintf(message->payload + message->payload_len + payload[0], sizeof(message->payload),
                        "%u", value);
    }
    case 1:
    {
        uint16_t short_real = ntohs(*((uint16_t *)payload));
        return snprintf(message->payload + message->payload_len, sizeof(message->payload),
                        "%.2f", short_real / 100.0);
    }
    case 2:
    {
        uint32_t long_real = ntohl(*((uint32_t *)(payload + 1)));
        uint8_t power = payload[5];
        double divisor = pow(10, power);
        double value = long_real / divisor;

        if (payload[0] != 0)
        {
            message->payload[message->payload_len] = '-';
        }
        return snprintf(message->payload + message->payload_len + payload[0], sizeof(message->payload),
                        "%.*f", (int)power, value);
    }
    case 3:
    {
        strncpy(message->payload + message->payload_len, payload, sizeof(message->payload));
        return strlen(payload);
    }
    }
    return 0;
}

bool topic_matches(const char *pattern, const char *topic)
{
    while (*pattern != '\0' && *topic != '\0')
    {
        if (*pattern == '+')
        {
            pattern++;
            while (*topic != '\0' && *topic != '/')
                topic++;
        }
        else if (*pattern == '*')
        {
            pattern++;
            if (*pattern == '\0')
                return true;
            while (*topic)
            {
                while (*topic != '\0' && *topic != '/')
                    topic++;
                if (*topic == '\0')
                    return false;
                if (topic_matches(pattern, topic))
                    return true;
                topic++;
            }
        }
        else if (*pattern != *topic)
        {
            return false;
        }
        else
        {
            pattern++;
            topic++;
        }
    }
    if (*pattern == '\0' && *topic == '\0')
        return true;
    return false;
}

void parse_topic(
    char *raw_topic,
    struct sockaddr_in client_addr,
    udp_payload *buffer,
    unordered_map<string, unordered_set<string>> &subscriptions,
    unordered_map<string, int> &id_to_fd)
{
    const char *payload_type[4] = {
        "INT",
        "SHORT_REAL",
        "FLOAT",
        "STRING"};

    topic_update message;
    uint32_t len = 0;

    char ip4[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip4, INET_ADDRSTRLEN);
    snprintf(message.preambule, sizeof(message.preambule),
             "%s:%d - ", ip4, ntohs(client_addr.sin_port));

    strncpy(message.topic.cells, raw_topic, 50);
    message.topic.cells[50] = '\0';
    message.topic.size = htonl(strlen(message.topic.cells));
    message.payload_len = snprintf(message.payload,
                                   sizeof(message.payload),
                                   "- %s - ", payload_type[buffer->ptype]);

    message.payload_len += parse_payload(buffer->message, buffer->ptype, &message);
    message.payload[message.payload_len++] = '\0';
    len += 30 + sizeof(topic_body) + sizeof(message.payload_len);
    len += message.payload_len;
    message.len = htonl(len);
    len += sizeof(len);
    message.payload_len = htonl(message.payload_len);

    for (auto &[id, topics] : subscriptions)
    {
        for (auto &sub_topic : topics)
        {
            if (topic_matches(sub_topic.c_str(), message.topic.cells))
            {
                if (id_to_fd.count(id))
                {
                    send_all(id_to_fd[id], &message, len);
                    cout << "sent " << message.topic.cells << " to: " << id << "\n";
                }
                break;
            }
        }
    }
}

void close_connections(unordered_map<int, string> &fd_to_id)
{
}

void close_connection(int fd)
{
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        error_exit("Port argument needed");
    int port = atoi(argv[1]);
    if (port == 0)
        error_exit("Invalid port value");

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd;
    struct sockaddr_in servaddr;
    udp_payload buffer;
    unordered_map<string, unordered_set<string>> subscriptions;
    unordered_map<int, string> fd_to_id;
    unordered_map<string, int> id_to_fd;

    // Create the two sockets
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
        error_exit("UDP socket creation failed");

    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
        error_exit("TCP socket creation failed");

    // Initialise address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // Bind sockets
    if (bind(udp_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        error_exit("UDP bind failed");

    if (bind(tcp_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        error_exit("TCP bind failed");

    // Since Linux 2.6.8, the size argument is ignored, but must be greater
    // than zero
    int epollfd = epoll_create(100);
    if (epollfd < 0)
        error_exit("epoll instance failed to be created");

    // TCP_NODELAY activate to stop Nagle algorithm
    int flag = 1;
    if (setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
        error_exit("setsockopt TCP_NODELAY failed");

    // Add sockets and STDIN to epollset
    struct epoll_event ev_tcp;
    ev_tcp.events = EPOLLIN;
    ev_tcp.data.fd = tcp_socket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, tcp_socket, &ev_tcp) < 0)
        error_exit("error adding to epoll");

    struct epoll_event ev_udp;
    ev_udp.events = EPOLLIN;
    ev_udp.data.fd = udp_socket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, udp_socket, &ev_udp) < 0)
        error_exit("error adding to epoll");

    struct epoll_event command_line;
    command_line.events = EPOLLIN;
    command_line.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &command_line) < 0)
        error_exit("error adding to epoll");

    if (listen(tcp_socket, SOMAXCONN) < 0)
        error_exit("TCP listen failed");

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
            int connfd = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_len);
            if (connfd < 0)
                error_exit("Error accepting connection");

            struct epoll_event conn;
            conn.events = EPOLLIN;
            conn.data.fd = connfd;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &conn) < 0)
                error_exit("error adding to epoll");

            uid id;
            recv_all(connfd, &id);
            string id_str = id.id;

            id_to_fd[id_str] = connfd;
            fd_to_id[connfd] = id_str;
            cout << id_str << " just joined\n";
        }
        else if (fd == udp_socket)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int len = recvfrom(udp_socket, &buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr *)&client_addr, &client_len);
            if (len < 0)
                error_exit("Error receiving UDP message");
            buffer.message[1500] = '\0';
            ;
            parse_topic(buffer.topic, client_addr, &buffer, subscriptions, id_to_fd);
        }
        else if (fd == STDIN_FILENO)
        {
            string input;
            cin >> input;
            if (input.compare("exit") == 0)
            {
                close_connections(fd_to_id);
                break;
            }
        }
        else
        {
            subscription sub;
            int rc = recv_all(fd, &sub);
            if (rc < 0)
                error_exit("Error receiving subscription from " + fd_to_id[fd]);
            else if (rc == 0)
            {
                close_connection(fd);
                continue;
            }

            sub.len = ntohl(sub.len);
            sub.topic.size = ntohl(sub.topic.size);

            if (sub.sub_state)
            {
                cout << "Subscribed to " << sub.topic.cells << '\n';
                subscriptions[fd_to_id[fd]].insert(sub.topic.cells);
            }
            else
            {
                cout << "Unsubscribed from " << sub.topic.cells << '\n';
                subscriptions[fd_to_id[fd]].erase(sub.topic.cells);
            }
        }
    }

    close(udp_socket);
    close(tcp_socket);
    close(epollfd);
}