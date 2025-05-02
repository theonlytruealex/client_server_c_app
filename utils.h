#include <stdint.h>

typedef struct __attribute__((packed)) uid {
    uint32_t size;
    char id[11];
} uid;

typedef struct __attribute__((packed)) topic_body {
    uint32_t size;
    char cells[51];
} topic_body;

typedef struct __attribute__((packed)) topic_update {
    uint32_t len;
    char preambule[30];
    topic_body topic;
    uint32_t payload_len;
    char payload[1520];
} topic_update;

typedef struct __attribute__((packed)) subscription {
    uint32_t len;
    uint8_t sub_state;
    topic_body topic;
} subscription;

typedef struct __attribute__((packed)) udp_payload
{
    char topic[50];
    uint8_t ptype;
    char message[1501];
} udp_payload;

void printHexBytes(char *data, uint32_t n);

void error_exit(const std::string& s); 

int epoll_add(int epollfd, int fd, void *ptr, int event);

int send_all(int sockfd, void *buffer, uint32_t len);

int recv_all(int sockfd, void *buffer);