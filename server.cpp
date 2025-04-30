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

// #include <iomanip> // for hex, setw, etc.

// void printHexBytes(char *data, size_t n) {
//     cout << hex << setfill('0');
//     for (size_t i = 0; i < n; ++i) {
//         cout << setw(2) << static_cast<int>(data[i]) << " ";
//     }
//     cout << dec << endl; // Reset to decimal
// }



class Tree {
private:
    unordered_set<int> client_ids;  // Using set for client IDs

public:
    unordered_map<string, Tree*> children;

    Tree* next_child(const string& name) {
        if (!children[name]) {
            children[name] = new Tree();
        }
        return children[name];
    }

    bool add_client(int id) {
        return client_ids.insert(id).second;
    }

    bool remove_client(int id) {
        return client_ids.erase(id);
    }

    void send_to_clients(const string& message) {
        return;
    }

    vector<Tree*> get_all_children() {
        vector<Tree*> child_refs;
        for (auto& pair : children) {
            child_refs.emplace_back(pair.second);
        }
        return child_refs;
    }

    ~Tree() {
        for (auto& [name, child] : children) {
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
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tcp_socket, &ev_tcp);

    struct epoll_event ev_udp;
    ev_udp.events = EPOLLIN;
    ev_udp.data.fd = udp_socket;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, udp_socket, &ev_udp);

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
            int connfd = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_len);
            if (connfd >= 0)
            {
                cout << "Accepted TCP connection\n";
                close(connfd);
            }
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
            int topic_len = parse_topic(buffer.topic);
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

    close(udp_socket);
    close(tcp_socket);
    close(epollfd);
}