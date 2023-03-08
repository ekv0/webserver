#include "thread_pool.hh"

thread_pool::thread_pool(size_t nthreads,size_t max_queue_capacity)
    : nthreads(nthreads),
    workers(std::vector<pthread_t>(nthreads)),
    max_queue_capacity(max_queue_capacity)
{
    if (pthread_mutex_init(&mutex,nullptr) < 0) {
        throw std::runtime_error("pthread_mutex_init error");
    }
    if (pthread_cond_init(&cond,nullptr) < 0) {
        throw std::runtime_error("pthread_cond_init error");
    }
    if (pthread_mutex_init(&counter_mtx,nullptr) < 0) {
        throw std::runtime_error("pthread_mutex_init error");
    }
    //thread creation
    //run in detach state
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) < 0) {
        throw std::runtime_error("pthread_attr_init error");
    }
    if (pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) < 0) {
        throw std::runtime_error("pthread_attr_setdetachstate error");
    }
    for (auto &tid : workers) {
        if (pthread_create(&tid,&attr,thrd_fn,this) < 0) {
            throw std::runtime_error("pthread_create error");
        }
    }
}

thread_pool::~thread_pool()
{
    //block until threads are killed
    block_with(std::bind(&thread_pool::exiter,this),nthreads);
    //destroy posix objects
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&counter_mtx);
}

void thread_pool::block()
{
    block_with(std::bind(&thread_pool::detector,this),1);
}

void thread_pool::block_with(std::function<void ()> f,size_t num_to_block)
{
    counter = 0;    //assume block_with() is called within only one thread, so no mutex needed now
    max_queue_capacity += num_to_block; //ensure queue capacity
    for (int i(0); i < num_to_block; ++i) {
        push(f);    //inject detecting staffs
    }
    //detect
    while (true) {
        pthread_mutex_lock(&counter_mtx);
        if (counter == num_to_block) {
            pthread_mutex_unlock(&counter_mtx);
            break;
        }
        pthread_mutex_unlock(&counter_mtx);
        sleep(1);
    }
    //restore queue capacity
    max_queue_capacity -= num_to_block;
}

using namespace std;
bool thread_pool::push(const std::function<void ()> &task)
{
    pthread_t tid = pthread_self();
    dbg("acquiring lock " + to_string(tid));
    pthread_mutex_lock(&mutex);
    dbg("lock acquired " + to_string(tid));
    if (queue.size() >= max_queue_capacity) {   //queue's size can be greater than max size when preprocess is not empty
        dbg("queue full " + to_string(tid));
        pthread_mutex_unlock(&mutex);
        dbg("lock released " + to_string(tid));
        return false;
    }
    dbg("about to push into queue " + to_string(tid));
    queue.push(task);
    dbg("pushed " + to_string(tid));
    pthread_mutex_unlock(&mutex);
    dbg("lock released " + to_string(tid));
    pthread_cond_signal(&cond);
    dbg("cond signaled " + to_string(tid));
    return true;
}

void *thread_pool::thrd_fn(void *arg)
{
    pthread_t tid = pthread_self();
    auto p = static_cast<thread_pool *>(arg);
    while (true) {
        pthread_testcancel();   //cancellation point
        dbg("worker acquiring lock " + to_string(tid));
        pthread_mutex_lock(&p->mutex);
        dbg("worker lock acquired " + to_string(tid));
        while (p->empty()) {
            dbg("worker wait for cond " + to_string(tid));
            pthread_cond_wait(&p->cond,&p->mutex);  //cancellation point
        }
        dbg("worker ensure q not empty " + to_string(tid));
        auto task = p->queue.front();
        p->queue.pop();
        dbg("worker fetch task " + to_string(tid));
        pthread_mutex_unlock(&p->mutex);
        dbg("worker released lock " + to_string(tid));

        pthread_testcancel();   //cancellation point
        //process
        dbg("worker about to work " + to_string(tid));
        task();
        dbg("worker work done. " + to_string(tid));
    }
    return nullptr;  //dummy return
}

void thread_pool::exiter()
{
    detector();
    //exit
    pthread_exit(nullptr);
}

void thread_pool::detector()
{
    pthread_mutex_lock(&counter_mtx);
    ++counter;
    pthread_mutex_unlock(&counter_mtx);
}