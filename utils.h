#include <stdint.h>
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

int parse_topic(char *payload);

void error_exit(const std::string& s); 

int epoll_add(int epollfd, int fd, void *ptr, int event);