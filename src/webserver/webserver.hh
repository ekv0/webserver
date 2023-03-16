#ifndef WEBSERVER_HH
#define WEBSERVER_HH

#include "useful.hh"
#include "thread_pool/thread_pool.hh"
#include "expirer/expirer.hh"
#include "logger/logger.hh"
#include "http_conn/http_conn.hh"
#include "epoller/epoller.hh"

#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unordered_set>
#include <stdexcept>
#include <memory>

//debug
#define DBG_MACRO_DISABLE
#include <dbg.h>

class webserver
{
public:
    webserver(
        //normal
        unsigned port,
        bool listen_ET,
        bool conn_ET,
        size_t max_event,
        int backlog,
        std::string root,
        const std::set<std::string> &index_pages,
        size_t max_connection,
        size_t accept_thread_num,
        //about expire
        size_t livetime_s,
        size_t check_interval_s,
        //logger
        bool enable_logger,
        logger::log_level log_level,
        std::string log_path,
        bool log_async,
        size_t log_queue_capacity,
        //thread pool
        size_t nthreads,
        size_t thread_pool_queue_capacity
    );
    ~webserver();
    //start socket listening and processing
    void start();

private:
    //hash function for http_conn
    //use fd as identity
    struct http_conn_ptr_hasher {
        size_t operator()(std::shared_ptr<http_conn> conn) const {
            //for debug
            if (!conn.get())
                throw std::runtime_error("shared_ptr<http_conn> is null");
            log_debug("from hasher: conn->fd() = " + std::to_string(conn->fd()));
            return static_cast<size_t>(conn->fd());
        }
    };
    static expirer<std::shared_ptr<http_conn>,http_conn_ptr_hasher> timer;
    //the function the thread dedicated for signal handling runs
    static void *signal_handler_thrd_fn(void *arg);
    //set fd in non-block mode
    static void set_nonblock(int fd);
    //send error http response by http code
    static void send_error_response(int fd,int code);

    unsigned port;
    epoller ep;
    std::string root;
    thread_pool tp;
    std::set<std::string> index_pages;
    size_t max_connection;
    int backlog;
    uint32_t listen_events;
    uint32_t conn_events;
    size_t accept_thread_num;

    int listenfd;

    void init_listenfd(int backlog);
    void init_event_mask(bool listen_ET,bool conn_ET);
    //accept handler is thread-safe because accept(), epoll_ctl() are all thread-safe
    void accept_handler();
    void close_handler(std::shared_ptr<http_conn> conn);
    void read_handler(std::shared_ptr<http_conn> conn);
    void write_handler(std::shared_ptr<http_conn> conn);
};

#endif //WEBSERVER_HH