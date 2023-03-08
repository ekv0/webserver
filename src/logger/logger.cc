#include "logger.hh"

using namespace std;

logger *logger::instance()
{
    static logger ins;
    return &ins;
}

void logger::init(const string log_path,log_level target_level, bool async, size_t max_queue_capacity)
{
    enabled = true;
    //create at O_APPEND mode, writes are atomic, no lock needed
    if ((fd = open(log_path.c_str(),O_CREAT | O_WRONLY | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
        throw runtime_error("open error");
    }
    this->async = async;
    //only one worker thread for log writing
    pool = new thread_pool(1,max_queue_capacity);
    this->target_level = target_level;
}

logger::~logger()
{
    close(fd);
    delete pool;
}

void logger::log(logger::log_level level,const string &msg)
{
    if (!enabled || level < target_level) {
        return;
    }
    if (!async) {
        log_write(level,msg);   //write right now
    }
    else {
        pool->push(std::bind(&logger::log_write,this,level,msg));    //add to task queue
    }
}

//do the real thing
void logger::log_write(logger::log_level level,const string &msg)
{
    //only one worker thread doing logging, so buffer can be reused by all logging tasks
    buffer.clear();
    //level
    append_level(level);
    //datetime
    append_datetime();
    //msg
    buffer.append(msg.c_str());
    buffer.append("\n");
    
    ssize_t len = buffer.len();
    ssize_t tem;
    // fprintf(stdout,"before write buffer.len() = %ld\n",buffer.len());
    for (size_t i(0); (tem = write(fd,buffer.base(),len)) != len && i < max_retry; ++i) {
        fprintf(stdout,"after write (error) buffer.len() = %ld\n",buffer.len());
        fprintf(stderr,"log write error! wrote %ld while %ld expected! retrying\n",tem,len);
        buffer.retrieved(tem);
    }
    // fprintf(stdout," after write buffer.len() = %ld\n",buffer.len());
}

void logger::append_level(log_level level)
{
    switch(level) {
        case INFO:
            buffer.append("[info]  ");
            break;
        case WARN:
            buffer.append("[warn]  ");
            break;
        case ERR:
            buffer.append("[error] ");
            break;
        case DEBUG:
            buffer.append("[debug] ");
            break;
    }
}

void logger::append_datetime()
{
    auto now = time(nullptr);
    auto tm = gmtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z: ", tm);
    buffer.append(buf);
}