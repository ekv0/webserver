#ifndef LOGGER_HH
#define LOGGER_HH

#include "scalable_buffer/scalable_buffer.hh"
#include "thread_pool/thread_pool.hh"

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include <string>
#include <queue>
#include <stdexcept>

class logger
{
public:
    enum log_level {
        DEBUG,
        INFO,
        WARN,
        ERR
    };
    
    static logger *instance();
    void init(const std::string log_path = "/var/log/webserver.log",log_level target_level = INFO, bool async = false, size_t max_queue_capacity = 512);
    ~logger();
    void log(log_level level,const std::string &msg);
    void log(log_level level,std::string &&msg) {
        log(level,msg);
    }
    void flush() {
        if (enabled) {
            pool->block();
        }
    }
    
private:
    logger() {}
    void append_level(log_level level);
    void append_datetime();
    void log_write(log_level level,const std::string &msg);

    bool enabled = false;
    int fd; //<for log file
    bool async;
    scalable_buffer buffer{4096};   //the size of one page on most machines
    thread_pool *pool;   //<for asynchronous logging;
    log_level target_level;
    const size_t max_retry = 5;
};

//useful
#define log_debug(X) logger::instance()->log(logger::DEBUG,(X))
#define log_info(X) logger::instance()->log(logger::INFO,(X))
#define log_warn(X) logger::instance()->log(logger::WARN,(X))
#define log_err(X) logger::instance()->log(logger::ERR,(X))

#endif //LOGGER_HH