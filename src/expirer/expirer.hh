#ifndef EXPIRER_HH
#define EXPIRER_HH

#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include <iostream>
#include <list>
#include <unordered_map>
#include <functional>

template<typename T,typename _Hash = std::hash<T>>
class expirer
{
public:
    expirer(size_t livetime_s = 60,size_t check_interval_s = 5);
    ~expirer();
    void add(const T &obj,std::function<void (T &,bool)> cb);
    void add(T &&obj,std::function<void (T &,bool)> cb) {
        add(obj,cb);
    }
    //get obj by id
    T *get(size_t id);
    //the user should be responsible for 'id' being identity
    bool activate_by_id(size_t id);
    //or let the hash function decide which to expire
    bool activate(const T &obj);
    bool activate(T &&obj) {
        return activate(obj);
    }

    //manually invalidate
    bool invalidate_by_id(size_t id);
    bool invalidate(const T &obj);
    bool invalidate(T &&obj) {
        return invalidate(obj);
    }
    void set_livetime(size_t expire_s);
    //without acquiring lock, just a hint
    size_t size() const {
        return lst.size();
    }

    //this should be registered explicitly by caller
    void sig_alarm();
    static unsigned set_alarm(unsigned t);

private:
    struct _node {
        time_t last_active;
        T obj;
        //expired is passed in to distinguish between manually invaliadation and time out expiration
        std::function<void (T &obj,bool expired)> call_back;

        _node(const T &obj,std::function<void (T &obj,bool)> cb) : obj(obj), call_back(cb), last_active(time(nullptr)) {}
    };
    std::list<_node> lst;
    std::unordered_map<T,decltype(lst.begin()),_Hash> mp;
    std::unordered_map<size_t,decltype(lst.begin())> id_mp;
    size_t livetime_s;    //<expire time in second
    size_t check_interval_s;    //<check interval in second
    pthread_mutex_t mutex;  //<provide protection for list when involked multi-threadedly
};

template<typename T,typename _Hash>
expirer<T,_Hash>::expirer(size_t livetime_s,size_t check_interval_s)
    : livetime_s(livetime_s),
    check_interval_s(check_interval_s)
{
    if (pthread_mutex_init(&mutex,nullptr) < 0) {
        throw std::runtime_error("pthread_mutex_init error");
    }
    set_alarm(check_interval_s);
}

template<typename T,typename _Hash>
expirer<T,_Hash>::~expirer()
{
    //cancel alarm
    set_alarm(0);
    pthread_mutex_destroy(&mutex);
}

template<typename T,typename _Hash>
void expirer<T,_Hash>::add(const T &obj,std::function<void (T &,bool)> cb)
{
    pthread_mutex_lock(&mutex);
    lst.emplace_front(obj,cb);
    id_mp[_Hash()(obj)] = mp[obj] = lst.begin();
    pthread_mutex_unlock(&mutex);
}

template<typename T,typename _Hash>
T *expirer<T,_Hash>::get(size_t id)
{
    //maybe
    if (id_mp.count(id) == 0) {
        // std::cerr << id << " is nullptr" << std::endl;
        return nullptr;
    }
    // std::cerr << id << " is not nullptr" << std::endl;
    // std::cerr << "return " << &(id_mp[id]->obj) << std::endl;
    T *addr;
    while ((uint64_t)(addr = &(id_mp[id]->obj)) <= (uint64_t)0x1000)
        std::cerr << "invalid addr: " << addr << std::endl;
    return addr;
}

template<typename T,typename _Hash>
bool expirer<T,_Hash>::activate_by_id(size_t id)
{
    if (id_mp.count(id) == 0) {
        return false;
    }
    return activate(id_mp[id]->obj);
}

template<typename T,typename _Hash>
bool expirer<T,_Hash>::activate(const T &obj)
{
    pthread_mutex_lock(&mutex);
    auto mp_it = mp.find(obj);
    if (mp_it == mp.end()) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    auto lst_it = mp_it->second;
    lst_it->last_active = time(nullptr);
    lst.splice(lst.begin(),lst,lst_it);
    pthread_mutex_unlock(&mutex);
    return true;
}

template<typename T,typename _Hash>
bool expirer<T,_Hash>::invalidate_by_id(size_t id)
{
    if (id_mp.count(id) == 0) {
        return false;
    }
    return invalidate(id_mp[id]->obj);
}

template<typename T,typename _Hash>
bool expirer<T,_Hash>::invalidate(const T &obj)
{
    pthread_mutex_lock(&mutex);
    auto mp_it = mp.find(obj);
    if (mp_it == mp.end()) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    auto lst_it = mp_it->second;
    if (lst_it->call_back) {
        lst_it->call_back(lst_it->obj,false);
    }
    id_mp.erase(_Hash()(lst_it->obj));
    lst.erase(lst_it);
    mp.erase(mp_it);
    pthread_mutex_unlock(&mutex);
    return true;
}

template<typename T,typename _Hash>
unsigned expirer<T,_Hash>::set_alarm(unsigned t)
{
    int old_errno = errno;
    errno = 0;
    unsigned ret = alarm(t);
    if (errno != 0) {
        throw std::runtime_error("alarm error");
    }
    errno = old_errno;
    return ret;
}

template<typename T,typename _Hash>
void expirer<T,_Hash>::set_livetime(size_t expire_s) {
    livetime_s = expire_s;
}

template<typename T,typename _Hash>
void expirer<T,_Hash>::sig_alarm()
{
    pthread_mutex_lock(&mutex);
    time_t now = time(nullptr);
    for (auto it(lst.rbegin()); it != lst.rend() && now - it->last_active >= livetime_s; ++it) {
        //involk user defined call back
        if (it->call_back) {
            it->call_back(it->obj,true);
        }
        //clean
        mp.erase(it->obj);
        id_mp.erase(_Hash()(it->obj));
        //to forward iterator
        auto fit = it.base();
        lst.erase(--fit);
    }
    pthread_mutex_unlock(&mutex);
    
    set_alarm(check_interval_s);
}

#endif //EXPIRER_HH