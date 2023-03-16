#ifndef SCALABLE_BUFFER_HH
#define SCALABLE_BUFFER_HH

#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#include <iostream>
#include <string>
#include <stdexcept>

//the buffer works like this:
//|_____(tail)********(head)___|
// (extends rightward)
//legend: _ for rubbish data(writable), * for useful data(readable), | for buffer bound

class scalable_buffer
{
public:
    scalable_buffer(size_t init_sz = 65536, double ratio = 1.5) : sz(init_sz < 2 ? 2 : init_sz), ratio(ratio) {
        if ((ptr = static_cast<char *>(malloc(sz))) == nullptr) {
            throw std::runtime_error("malloc error");
        }
    }
    scalable_buffer(const scalable_buffer &oth) : ratio(oth.ratio) {
        copy(oth);
    }
    ~scalable_buffer() {
        free(ptr);
    }
    scalable_buffer &operator=(const scalable_buffer &oth) {
        if (ratio != oth.ratio) {
            throw std::runtime_error("scalable_buffer assignment to a different ratio!");
        }
        copy(oth);
        return *this;
    }

    //append to the end
    void append(const char *buf,size_t len);
    void append(const char *buf) {
        append(buf,strlen(buf));
    }
    void append(const std::string &str) {
        append(str.c_str());
    }
    size_t readable() const;
    size_t writable() const;
    //client has appended or retrieved some data; move ptrs only
    void appended(size_t n);
    void retrieved(size_t n);

    //read from fd ONCE, returns the last result from readv()
    ssize_t read_fd(int fd);
    //reset
    void clear();
    //readble part ptr
    char *base() const {
        return ptr + tail;
    }
    //alias to readable()
    size_t len() const {
        return readable();
    }

private:
    char *ptr;
    //buffer size
    size_t sz;
    //write offset
    size_t head = 0;
    //read offset
    size_t tail = 0;
    const double ratio;
    static const size_t aux_buffer_sz;

    void copy(const scalable_buffer &oth);
    //use the same auto-resize technology as STL vector
    //apply MSVC2015's 1.5 as the default ratio
    void resize() {
        if ((ptr = static_cast<char *>(realloc(ptr,sz *= ratio))) == nullptr) {
            throw std::runtime_error("realloc error");
        }
    }
    //number of bytes remained available
    size_t avail() {
        return sz - head;
    }
};

#endif //SCALABLE_BUFFER_HH