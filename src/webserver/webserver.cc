#include "webserver.hh"

using namespace std;

//block calling thread at the beginning of program running
static auto blocked_sigset = [](){
    sigset_t set;
    if (sigemptyset(&set) < 0) {
        log_err("sigemptyset failed");
        throw runtime_error("sigemptyset error");
    }
    if (sigaddset(&set,SIGALRM) < 0 || sigaddset(&set,SIGINT) < 0 || sigaddset(&set,SIGQUIT) < 0) {
        log_err("sigaddset failed");
        throw runtime_error("sigaddset error");
    }
    if (pthread_sigmask(SIG_BLOCK,&set,nullptr) != 0) {
        log_err("pthread_sigmask failed");
        throw runtime_error("sigprocmask error");
    }
    return set;
}();

expirer<shared_ptr<http_conn>,webserver::http_conn_ptr_hasher> webserver::timer;

webserver::webserver(
    unsigned port,
    bool listen_ET,
    bool conn_ET,
    size_t max_event,
    int backlog,
    std::string root,
    const std::set<std::string> &index_pages,
    size_t max_connection,
    size_t accept_thread_num,
    size_t livetime_s,
    size_t check_interval_s,
    bool enable_logger,
    logger::log_level log_level,
    std::string log_path,
    bool log_async,
    size_t log_queue_capacity,
    size_t nthreads,
    size_t thread_pool_queue_capacity
) : port(port),
    ep(max_event),
    backlog(backlog),
    root(root),
    index_pages(index_pages),
    max_connection(max_connection),
    accept_thread_num(accept_thread_num),
    tp(nthreads,thread_pool_queue_capacity)
{
    //dedicate another thread fro SIGALRM handling
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) < 0) {
        throw std::runtime_error("pthread_attr_init error");
    }
    if (pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) < 0) {
        throw std::runtime_error("pthread_attr_setdetachstate error");
    }
    pthread_t tid;
    if (pthread_create(&tid,&attr,signal_handler_thrd_fn,this) < 0) {
        throw std::runtime_error("pthread_create error");
    }
    //set timer
    timer = expirer<shared_ptr<http_conn>,http_conn_ptr_hasher>(livetime_s,check_interval_s);
    //alarm
    expirer<http_conn *,http_conn_ptr_hasher>::set_alarm(check_interval_s);

    //set http connection trigger mode
    http_conn::set_trigger(conn_ET);
    //set default epoll event mask
    init_event_mask(listen_ET,conn_ET);
    //set logger with logging thread SIGALRM blocked if async is true
    if (enable_logger) {
        logger::instance()->init(log_path,log_level,log_async,log_queue_capacity);
    }

    //log
    log_info("=====================================================");
    log_info("webserver listening at " + to_string(port));
    log_info("listen fd trigger mode: " + string(listen_ET ? "ET" : "LT"));
    log_info("client fd trigger mode: " + string(conn_ET ? "ET" : "LT"));
    log_info("max_event = " + to_string(max_event) + ", epoll_wait backlog = " + to_string(backlog));
    log_info("root directory = " + root);
    string stridxpage;
    for (auto &p : index_pages) {
        stridxpage += p + ", ";
    }
    stridxpage.pop_back();
    stridxpage.pop_back();
    log_info("index pages: " + stridxpage);
    log_info("max_connection = " + to_string(max_connection));
    log_info("number of threads accepting connection requests = " + to_string(listen_ET ? 1 : accept_thread_num));
    log_info("connection livetime = " + to_string(livetime_s) + "s, check interval = " + to_string(check_interval_s) + "s");
    log_info("logger " + string(enable_logger ? "enabled" : "disabled"));
    if (enable_logger) {
        log_info("\tlog path = " + log_path + ", logging mode = " + string(log_async ? "async" : "sync"));
    }
    log_info("number of working threads = " + to_string(nthreads) + ", task queue capacity = " + to_string(thread_pool_queue_capacity));
    log_info("=====================================================");
}

webserver::~webserver()
{
    close(listenfd);
}

void webserver::start()
{
    log_info("webserver starting...");
    init_listenfd(backlog);
    ep.add(listenfd,listen_events);
    log_info("ready to serve");
    //this main thread is the reactor/dispatcher
    auto events = ep.events();
    while (true) {
        size_t n = ep.wait(-1);
        log_debug("returned from epoll wait. n = " + to_string(n));
        for (size_t i(0); i < n; ++i) {
            log_debug("i = " + to_string(i));
            auto fd = events[i].data.fd;
            log_debug("got event fd = " + to_string(fd));
            auto pconn = timer.get(fd);
            log_debug("got pconn");
            string ipport("unknown host");  //to avoid expiration
            if (pconn) {
                ipport = str_ipport((*pconn)->addr());
            }

            auto ev = events[i].events;
            if (events[i].data.fd == listenfd) {
                log_debug("\taccept event");
                //if listenfd is in ET mode, then accept multi-threadedly!!!
                size_t i(0);
                do {
                    tp.push(bind(&webserver::accept_handler,this));
                    log_debug(string("\t") + "accept task pushed");
                    ++i;
                } while ((listen_events & EPOLLET) && i < accept_thread_num);
            }
            //peer close or error encounter
            else if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                log_debug("\tpeer close or error event");
                if (ev & EPOLLERR) {
                    log_err("Error condition happened on the associated connection: " + ipport + ". events = " + to_string(ev) + ". closing...");
                }
                else if (ev & EPOLLRDHUP) {
                    log_info("Stream socket peer closed connection, or shut down writing half of connection: " + ipport);
                }
                else {
                    log_info("Hang up happened on the associated connection: " + ipport);
                }
                tp.push(bind(&webserver::close_handler,this,*pconn));
                log_debug("\t" + ipport + " close task pushed");
            }
            //read
            else if (ev & EPOLLIN) {
                log_debug("\tread event");
                //activate first
                timer.activate(*pconn);
                log_debug("\t" + ipport + " activated");
                tp.push(bind(&webserver::read_handler,this,*pconn));
                log_debug("\t" + ipport + " read task pushed");
            }
            //write
            else if (ev & EPOLLOUT) {
                log_debug("\twrite event");
                timer.activate(*pconn);
                log_debug("\t" + ipport + " activated");
                tp.push(bind(&webserver::write_handler,this,*pconn));
                log_debug("\t" + ipport + " write task pushed");
            }
            else {
                log_debug("\tunexpected event");
                log_err("unexpected epoll event: " + to_string(ev));
            }
        }
    }
}

void webserver::accept_handler()
{
    do {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int clientfd = accept(listenfd,(struct sockaddr *)&addr,&len);
        if (clientfd < 0) {
            break;
        }
        auto ipport = str_ipport(addr);
        log_info("accept connection from " + ipport);
        if (http_conn::conn_count() >= max_connection) {
            //send 503
            tp.push(bind(send_error_response,clientfd,503));
            log_warn("connection from " + ipport + " is rejected due to server busy");
            continue;
        }
        set_nonblock(clientfd);
        //construct a new http connection and time it
        auto sp = make_shared<http_conn>(clientfd,addr,root,index_pages);
        timer.add(sp,[ipport,this](shared_ptr<http_conn> conn,bool expired){
            if (expired) {
                log_info("connection from " + ipport + " timeout. closing");
            }
            else {
                log_info("close connection from " + ipport);
            }
            ep.del(conn->fd());
        });   //no call back is needed, as http_conn's destructor will do anything necessary
        log_debug(ipport + " added to timer");
        //then add to interest list
        ep.add(clientfd,conn_events | EPOLLIN);
        log_debug(ipport + " added to IN list");
    } while ((listen_events & EPOLLET));
}

void webserver::close_handler(shared_ptr<http_conn> conn)
{
    timer.invalidate(conn);
}

void webserver::read_handler(shared_ptr<http_conn> conn)
{
    //if connectin expired during waiting for served
    if (!conn) {
        return;
    }
    //if not expired, then it's likely to remain valid until writable
    auto ipport = str_ipport(conn->addr());
    if (conn->read() < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        tp.push(bind(&webserver::close_handler,this,conn));
        log_debug("error read in " + ipport + ", close task pushed");
        log_err("close " + ipport + " due to error read");
        return;
    }
    if (conn->ready_for_write()) {
        log_debug("connection from " + ipport + " is ready for write, add to OUT list");
        ep.mod(conn->fd(),conn_events | EPOLLOUT);
    }
    else {
        log_debug("connection from " + ipport + "is yet not ready for write, add to In list");
        ep.mod(conn->fd(),conn_events | EPOLLIN);  //re-register because client fd's are in EPOLLONESHOT
    }
}

void webserver::write_handler(shared_ptr<http_conn> conn)
{
    //if connectin expired during waiting for served
    if (!conn) {
        return;
    }
    //if not expired, then it's likely to remain valid until writable
    auto ipport = str_ipport(conn->addr());
    auto len = conn->write();
    //complete
    if (len == 0) {
        log_debug("write to connection from " + ipport + " completed");
        if (conn->persistent()) {
            //wait for next read event
            log_debug("connection from " + ipport + " is persistent, add to IN list");
            conn->reset();
            ep.mod(conn->fd(),conn_events | EPOLLIN);
        }
        else {
            //close
            log_debug("connection from " + ipport + " is not persistent, closing");
            timer.invalidate(conn);
        }
    }
    //in ET mode
    else if (conn_events & EPOLLET) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            //write would block or interrupted
            log_debug("write to " + ipport + " was interrupted or would block, add to OUT list");
            ep.mod(conn->fd(),conn_events | EPOLLOUT);
        }
        else {
            log_debug("close connection from " + ipport + " due to write error");
            timer.invalidate(conn);
        }
    }
    //in LT mode
    else if (len > 0) {
        log_debug("connectino from " + ipport + " is in LT, add to OUT list");
        //all data may not be written
        ep.mod(conn->fd(),conn_events | EPOLLOUT);
    }
    //in LT mode and len < 0
    else {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            log_debug("write to " + ipport + " was interrupted or would block, add to OUT list");
            ep.mod(conn->fd(),conn_events | EPOLLOUT);
        }
        else {
            log_debug("connectino from " + ipport + " is in LT, add to OUT list");
            ep.mod(conn->fd(),conn_events | EPOLLOUT);
        }
    }
}

void webserver::init_event_mask(bool listen_ET,bool conn_ET)
{
    listen_events = EPOLLIN;    //always read
    if (listen_ET) {
        listen_events |= EPOLLET;
    }
    conn_events = EPOLLRDHUP | EPOLLONESHOT;
    if (conn_ET) {
        conn_events |= EPOLLET;
    }
}

void webserver::init_listenfd(int backlog)
{
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if (listenfd < 0) {
        log_err("listen socket creation init failed");
        throw runtime_error("socket error");
    }
    //set non-block
    set_nonblock(listenfd);

    sockaddr_in to_bind;
    to_bind.sin_family = AF_INET;
    to_bind.sin_addr.s_addr = htonl(INADDR_ANY);
    to_bind.sin_port = htons(port);
    //bind
    if (::bind(listenfd,(struct sockaddr *)&to_bind,sizeof(to_bind)) < 0) {
        log_err("listen socket bind failed");
        throw runtime_error("bind error");
    }
    //listen
    if (listen(listenfd,backlog) < 0) {
        log_err("listen socket listen failed");
        throw runtime_error("listen error");
    }
}

void webserver::set_nonblock(int fd)
{
    auto saved = errno;
    auto flag = fcntl(fd,F_GETFL,0);
    if (flag == -1) {
        log_err("set " + to_string(fd) + " non-block failed");
        throw runtime_error("fcntl error");
    }
    if (fcntl(fd,F_SETFL,flag | O_NONBLOCK) == -1) {
        log_err("fcntl failed");
        throw runtime_error("fcntl error");
    }
    errno = saved;
}

void *webserver::signal_handler_thrd_fn(void *arg)
{
    auto ins = static_cast<webserver *>(arg);
    //handling SIGALRM
    while (true) {
        int signo;
        if (sigwait(&blocked_sigset, &signo) != 0) {
            log_err("sigwait failed");
            throw runtime_error("sigprocmask error");
        }
        switch (signo) {
            case SIGALRM:
                // dbg("about to broadcast");
                // ins->tp.broadcast();
                // dbg("broadcast done","about to run timer.sig_alarm()");
                timer.sig_alarm();
                log_info("current active connection: " + to_string(timer.size()));
                break;
            case SIGINT:
            case SIGQUIT:
                log_info(string((signo == SIGINT) ? "SIGINT" : "SIGQUIT") + " received. exiting...");
                //wait for all jobs done
                ins->tp.block();
                dbg("thread pool block return");
                logger::instance()->flush();
                dbg("logger flush return");
                ins->~webserver();
                dbg("destructed");
                //terminate process
                ::exit(0);
                break;
            default:
                log_err("unexpected signal: " + to_string(signo));
        }
    }
}

void webserver::send_error_response(int fd,int code)
{
    scalable_buffer buf(4096);
    http_response res;
    res.init(code,buf);
    //blocking write
    do {
        auto len = write(fd,buf.base(),buf.readable());
        buf.retrieved(len);
    } while (buf.readable());
    close(fd);
}