#include "scalable_buffer.hh"

using namespace std;

const size_t scalable_buffer::aux_buffer_sz = 65536;

void scalable_buffer::append(const char *buf,size_t len)
{
    while (avail() < len) {
        resize();
    }
    memcpy(ptr + head,buf,len);
    head += len;
}

size_t scalable_buffer::readable() const
{
    return head - tail;
}

size_t scalable_buffer::writable() const
{
    return sz - head;
}

void scalable_buffer::appended(size_t n)
{
    if (n > writable()) {   //the client write over the memory bound
        // throw runtime_error("scalable_buffer write overflow");
        std::cerr << "scalable_buffer write overflow" << std::endl;
    }
    head += n;
}

void scalable_buffer::retrieved(size_t n)
{
    if (n > readable()) {    //the client read over valid region
        // throw runtime_error("scalable_buffer read overflow");
        std::cerr << "scalable_buffer read overflow" << std::endl;
    }
    tail += n;
    if (tail == head) { //to save some space when no data presents
        clear();
    }
}

ssize_t scalable_buffer::read_fd(int fd)
{
    struct iovec iov[2];
    iov[0].iov_base = static_cast<char *>(ptr) + head;
    iov[0].iov_len = avail();
    char aux[aux_buffer_sz];
    iov[1].iov_base = aux;
    iov[1].iov_len = aux_buffer_sz;
    ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        return len; //indicate error
    }
    else if (len <= avail()) {
        head += len;
    }
    else if (len > avail()) {
        auto need = len - avail();
        head = sz;
        do {
            resize();
        } while (avail() < need);
        memcpy(static_cast<char *>(ptr) + head,aux,need);
        head += need;
    }
    return len;
}

void scalable_buffer::clear()
{
    tail = head = 0;
}

void scalable_buffer::copy(const scalable_buffer &oth)
{
    ptr = new char[oth.len()];
    if (!ptr) {
        throw std::runtime_error("malloc error");
    }
    if (!memcpy(ptr,oth.base(),oth.len())) {
        throw std::runtime_error("memcpy error");
    }
    sz = oth.sz;
    head = oth.head;
    tail = oth.tail;
}