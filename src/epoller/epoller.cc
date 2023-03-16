#include "epoller.hh"

epoller::epoller(size_t max_event) : max_event(max_event)
{
    events_ = new epoll_event[max_event];
    if (!events_) {
        throw std::runtime_error("new error");
    }
    int save = errno;
    errno = 0;
    epfd = epoll_create(1);
    if (errno != 0) {
        log_err("epoll_create error: " + std::string(strerror(errno)));
        throw std::runtime_error("epoll_create error: " + std::string(strerror(errno)));
    }
    errno = save;
}

void epoller::add(int fd,uint32_t e)
{
    ctl(fd,EPOLL_CTL_ADD,e);
}

void epoller::mod(int fd,uint32_t e)
{
    ctl(fd,EPOLL_CTL_MOD,e);
}

void epoller::del(int fd)
{
    ctl(fd,EPOLL_CTL_DEL,0);
}

void epoller::ctl(int fd,int op,uint32_t events)
{
    epoll_event ev;
    if (op != EPOLL_CTL_DEL) {
        ev.data.fd = fd;
        ev.events = events;
    }
    int save = errno;
    if (epoll_ctl(epfd,op,fd,(op == EPOLL_CTL_DEL) ? nullptr : &ev) == -1) {
        log_err("epoll_ctl error: " + std::string(strerror(errno)));
        throw std::runtime_error("epoll_ctl error: " + std::string(strerror(errno)));
    }
    errno = save;
}

size_t epoller::wait(int timeout)
{
    int save = errno;
    int val = epoll_wait(epfd,events_,max_event,timeout);
    if (val == -1) {
        log_err("epoll_wait error: " + std::string(strerror(errno)));
        throw std::runtime_error("epoll_wait error: " + std::string(strerror(errno)));
    }
    errno = save;
    return val;
}