#ifndef HTTP_CONN_HH
#define HTTP_CONN_HH

#include "useful.hh"
#include "http_request/http_request.hh"
#include "http_response/http_response.hh"
#include "logger/logger.hh"
#include "scalable_buffer/scalable_buffer.hh"

#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <string>
#include <unordered_set>
#include <stdexcept>

//debug
// #define DBG_MACRO_DISABLE
#include <dbg.h>

class http_conn
{
public:
    http_conn(int fd,const sockaddr_in &client_addr,std::string root,const std::set<std::string> &index_pages);
    http_conn(int fd,const sockaddr_in &client_addr,std::string root,std::set<std::string> &&index_pages) : http_conn(fd,client_addr,root,index_pages) {}
    http_conn(int fd,const sockaddr_in &client_addr,std::string root) : http_conn(fd,client_addr,root,{}) {}
    ~http_conn();
    //statically set trigger mode
    static void set_trigger(bool ET);
    static bool get_trigger();
    //read from fd once or multiple times depending on ET, returns the result of last scalable_buffer::read_fd()
    ssize_t read();
    //check whether request received is ready for generating response
    bool ready_for_write();
    //write to fd once or multiple times depending on ET, returns the result of last writev()
    ssize_t write();
    //reset for next HTTP request
    void reset() {
        request.reset();
        rw_buf.clear();
    }
    //tell if connection is persistent
    //must be called after ready_for_write returns true
    bool persistent() const {
        return http_persistent;
    }
    int fd() const {
        return _fd;
    }
    sockaddr_in addr() const {
        return client_addr;
    }

    //not using lock; just a hint
    static size_t conn_count() {
        return n_conn;
    }

private:
    int _fd;
    sockaddr_in client_addr;
    http_request request;
    http_response response;
    bool http_persistent;
    scalable_buffer rw_buf{4096};   //<4KB initial size, as big as one page on most machines
    struct iovec iov[2];
    std::string root;
    std::set<std::string> index_pages;

    static bool ET;
    static size_t n_conn;
    static pthread_mutex_t mutex;
    
    //increase/decrease connection count
    static void incr_conn();
    static void decr_conn();
};

#endif //HTTP_CONN_HH