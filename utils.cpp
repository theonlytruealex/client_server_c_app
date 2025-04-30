#include <iostream>
#include <sys/epoll.h>
using namespace std;

int parse_topic(char *payload)
{
    char topic[51];
    int i;
    for (i = 0; i < 50; i++)
    {
        topic[i] = payload[i];
        if (payload[i] == '\0')
            break;
    }
    topic[50] = '\0';
    cout << topic;
    // take the last \0 into account if it exists
    if (i < 50)
        i++;
    return i;
}

void error_exit(const std::string& s) 
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
