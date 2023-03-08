#ifndef EPOLLER_HH
#define EPOLLER_HH

#include "logger/logger.hh"

#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <stdexcept>

//this class stores fd into epoll_event.data

class epoller
{
public:
    epoller(size_t max_event);
    ~epoller() {
        close(epfd);
        delete[] events_;
    }
    void add(int fd,uint32_t e);
    void mod(int fd,uint32_t e);
    void del(int fd);
    size_t wait(int timeout);
    epoll_event *events() {
        return events_;
    }

private:
    int epfd;
    epoll_event *events_;
    size_t max_event;

    void ctl(int fd,int op,uint32_t events);
};

#endif //EPOLLER_HH