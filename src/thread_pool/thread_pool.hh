#ifndef THREAD_POOL_HH
#define THREAD_POOL_HH

#include <stdlib.h>
#include <pthread.h>

#include <iostream>
#include <queue>
#include <vector>
#include <stdexcept>
#include <functional>

//debug
// #define DBG_MACRO_DISABLE
#include <dbg.h>
#include <string>

class thread_pool
{
public:
    thread_pool(size_t nthreads = 16,size_t max_queue_capacity = 65536);
    ~thread_pool();
    bool push(const std::function<void ()> &task);
    bool push(std::function<void ()> &&task) {
        return push(task);
    }
    //when called with mutex lock acquired, like thrd_fn() does, the result is accurate
    //when called without lock acquired, it's just a hint
    bool empty() const {
        return queue.empty();
    }
    size_t thread_num() const {
        return workers.size();
    }
    //block until all tasks currently in the queue are done
    //if user push tasks asynchronously (i.e. pushing tasks in thread(s) other than the thread(s) calling block()), the tasks waited may differ than those when block() is called due to thread scheduling
    void block();
    void broadcast() {
        pthread_cond_broadcast(&cond);
    }

private:
    size_t nthreads;
    std::vector<pthread_t> workers;
    std::queue<std::function<void ()>> queue;
    size_t max_queue_capacity;
    //for queue
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    size_t counter;
    pthread_mutex_t counter_mtx; //for n_exit
    void detector();
    void exiter();
    void block_with(std::function<void ()> f,size_t num_to_block);

    static void *thrd_fn(void *arg);
};

#endif //THREAD_POOL_HH