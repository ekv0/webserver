#include "http_conn.hh"

using namespace std;

size_t http_conn::n_conn = 0;
pthread_mutex_t http_conn::mutex = PTHREAD_MUTEX_INITIALIZER;

http_conn::http_conn(int fd,const sockaddr_in &client_addr,std::string root,const std::set<std::string> &index_pages)
    : _fd(fd),
    client_addr(client_addr),
    root(root),
    index_pages(index_pages)
{
    incr_conn();
}

void http_conn::incr_conn()
{
    pthread_mutex_lock(&mutex);
    ++n_conn;
    pthread_mutex_unlock(&mutex);
}

void http_conn::decr_conn()
{
    pthread_mutex_lock(&mutex);
    --n_conn;
    pthread_mutex_unlock(&mutex);
}

//default LT
bool http_conn::ET = false;

void http_conn::set_trigger(bool ET)
{
    http_conn::ET = ET;
}

bool http_conn::get_trigger()
{
    return ET;
}

http_conn::~http_conn()
{
    close(_fd);
    decr_conn();
}

ssize_t http_conn::read()
{
    ssize_t len;
    do {
        len = rw_buf.read_fd(fd());
    } while (len > 0 && ET);    //when in ET, read all or until interrupted
    //debug log
    rw_buf.base()[rw_buf.len()] = 0;
    log_debug("received from " + str_ipport(client_addr) + ":\n" + rw_buf.base());
    return len;
}

bool http_conn::ready_for_write()
{
    auto state = request.parse(rw_buf);
    if (state == request.FINISH || state == request.SYNTAX_ERROR) {
        http_persistent = request.persistent();
        return true;
    }
    return false;
}

ssize_t http_conn::write()
{
    //generate response using request
    if (index_pages.empty()) {
        response.init(request,rw_buf,root);
    }
    else {
        response.init(request,rw_buf,root,index_pages);
    }
    //get generated status line and header lines
    iov[0].iov_base = rw_buf.base();
    iov[0].iov_len = rw_buf.len();
    //get body
    auto body = response.body();
    iov[1].iov_base = body.first;
    iov[1].iov_len = body.second;
    //log
    rw_buf.base()[rw_buf.len()] = 0;
    log_debug("response generated for " + str_ipport(client_addr) + ":\n" + rw_buf.base() + (body.first ? body.first : ""));
    //write
    ssize_t len;
    do {
        len = writev(fd(),iov,2);
        //update iovec len
        if (len > iov[0].iov_len) { //iov[0].iov_len must be greater than 0 after subtraction
            len -= iov[0].iov_len;
            iov[0].iov_len = 0;
            iov[1].iov_len -= len;
            iov[1].iov_base = static_cast<char *>(iov[1].iov_base) + len;
        }
        else if (len > 0) {
            iov[0].iov_len -= len;
            iov[0].iov_base = static_cast<char *>(iov[0].iov_base) + len;
        }
        else {  //len == 0 or len < 0
            break;
        }
    } while (ET);    //when in ET, write all or until interrupted
    return len;
}