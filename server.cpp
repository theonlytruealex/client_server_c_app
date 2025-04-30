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

using namespace std;

class Tree
{
public:
    unordered_map<string, Tree *> children;
    bool end;

    Tree *add_child(const string &name)
    {
        if (!children[name])
        {
            children[name] = new Tree();
        }
        return children[name];
    }

    vector<Tree *> get_all_children()
    {
        vector<Tree *> child_refs;
        for (auto &pair : children)
        {
            child_refs.emplace_back(pair.second);
        }
        return child_refs;
    }

    Tree()
    {
        end = false;
    }

    ~Tree()
    {
        for (auto &[name, child] : children)
        {
            delete child;
        }
    }
};

int parse_payload(char *payload, uint8_t ptype)
{
    switch (ptype)
    {
    case 0:
    {
        if (payload[0] != 0)
            cout << "-";
        unsigned int *integer = (unsigned int *)(payload + 1);
        cout << ntohl(integer[0]);
        return 5;
    }
    case 1:
    {
        uint16_t short_real = ntohs(((uint16_t *)(payload))[0]);
        cout << short_real / 100 << ".";
        if (short_real % 100 < 10)
            cout << 0;
        cout << short_real % 100;
        return 3;
    }
    case 2:
    {
        if (payload[0] != 0)
            cout << "-";
        uint8_t power = (uint8_t)payload[5];
        int power_ten = 1;
        for (int i = 0; i < power; i++)
            power_ten *= 10;
        uint32_t long_real = ntohl(((uint32_t *)(payload + 1))[0]);
        cout << long_real / power_ten << ".";
        while (long_real % power_ten < power_ten / 10)
        {
            cout << 0;
            power_ten /= 10;
        }
        cout << long_real % power_ten;
        return 6;
    }
    case 3:
        cout << payload;
    }
    return 1 + strlen(payload);
}

void parse_topic(char *payload, Tree *root, ssize_t max_len)
{
    int i;
    string part;
    for (i = 0; i < max_len; i++)
    {
        if (payload[i] == '\0' || payload[i] == '/')
            break;
        part.push_back(payload[i]);
    }

    if (part.length() == 1 && part[0] == '+')
    {
        auto children = root->get_all_children();
        for (auto &child : children)
        {
            parse_topic(payload + 2, child, max_len - 2);
        }
    }
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
    unordered_map<int, char *> fd_to_id;
    unordered_map<char *, int> id_to_fd;
    vector<pair<string, Tree *>> subscriptions;

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

    // Add sockets and STDIN to epoll
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

            ssize_t id_len;
            recv_all(connfd, &id_len, sizeof(id_len));
            char *id = (char *)malloc(id_len);
            recv_all(connfd, id, id_len);

            id_to_fd[id] = connfd;
            fd_to_id[connfd] = id;
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
            char ip4[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), ip4, INET_ADDRSTRLEN);
            cout << ip4 << ":" << ntohs(client_addr.sin_port) << " - ";
            parse_topic(buffer.topic, root, 50);
            cout << " " << payload_type[buffer.ptype] << " ";
            int payload_len = parse_payload(buffer.message, buffer.ptype);
            cout << '\n';
        }
        else if (fd == STDIN_FILENO)
        {
            string input;
            cin >> input;
            if (input.compare("exit") == 0)
                break;
        }
    }

    delete root;
    close(udp_socket);
    close(tcp_socket);
    close(epollfd);
}