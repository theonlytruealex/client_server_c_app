#include <stdint.h>

typedef struct topic_update {
    size_t len;
    char payload[1587];
} topic_update;

typedef struct subscription {
    int sub_state;
    char topic[51];
} subscription;

const char *payload_type[4] = {
    "INT",
    "SHORT_REAL",
    "FLOAT",
    "STRING"};

typedef struct __attribute__((packed)) udp_payload
{
    char topic[50];
    uint8_t ptype;
    char message[1501];
} udp_payload;

void error_exit(const std::string& s); 

int epoll_add(int epollfd, int fd, void *ptr, int event);

void send_all(int sockfd, void *buffer, size_t len);

void recv_all(int sockfd, void *buffer, size_t len);