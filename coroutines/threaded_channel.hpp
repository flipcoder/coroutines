// Copyright (c) 2013 Maciej Gajewski
#ifndef COROUTINES_NAIVE_CHANNEL_HPP
#define COROUTINES_NAIVE_CHANNEL_HPP

#include <memory>
#include <vector>
#include <condition_variable>
#include <cassert>
#include <utility>
#include <iostream> // for debugging

#include "channel.hpp"
#include "mutex.hpp"
#include "spsc_queue.hpp"

namespace coroutines {


// simple channel
template<typename T, typename ConditionVariable>
class threaded_channel : public i_writer_impl<T>, public i_reader_impl<T>
{
public:

    // factory
    static channel_pair<T> make(std::size_t capacity)
    {
        std::shared_ptr<threaded_channel<T, ConditionVariable>> me(new threaded_channel<T, ConditionVariable>(capacity));
        return channel_pair<T>(channel_reader<T>(me), channel_writer<T>(me));
    }

    threaded_channel(const threaded_channel&) = delete;

    // called by producer
    virtual void put(T v) override;
    virtual void writer_close() override;

    // caled by consumer
    virtual T get() override;
    virtual void reader_close();


private:

    threaded_channel(std::size_t capacity);
    void do_close();

    spsc_queue<T> _queue;

    mutex _read_mutex, _write_mutex;
    ConditionVariable _cv;

    bool _closed = false;
};

template<typename T, typename ConditionVariable>
threaded_channel<T, ConditionVariable>::threaded_channel(std::size_t capacity)
    : _queue(capacity+1)
{
}


template<typename T, typename ConditionVariable>
void threaded_channel<T, ConditionVariable>::put(T v)
{
    std::lock_guard<mutex> lock(_write_mutex);

    // try to insert without locking
    if (!_queue.put(v))
    {
        // failed, locking (and possiibly waiting) needed
        _write_mutex.unlock();
        std::lock(_write_mutex, _read_mutex);
        //std::cout << "PUT: waiting" << std::endl;
        _cv.wait(_read_mutex, [this, &v](){ return  _queue.put(v) || _closed; });
        //std::cout  << "PUT: resuming" << std::endl;
        _read_mutex.unlock();
    }

    if (_closed)
        throw channel_closed();

    //std::cout << "PUT: q size after put: " << _queue.size() << "/" << _queue.capacity() << std::endl;
    if(_queue.size() == 1)
    {
        //std::cout << "PUT: waking up consumer" << std::endl;
        _cv.notify_all();
    }
}

template<typename T, typename ConditionVariable>
T threaded_channel<T, ConditionVariable>::get()
{
    std::lock_guard<mutex> lock(_read_mutex);
    T result;
    bool success = true;

    // try to read wihtout blocking
    if(!_queue.get(result))
    {
        // failed, locking (and possiibly waiting) needed
        _read_mutex.unlock();
        std::lock(_read_mutex, _write_mutex);

        // wait for the queue to be filled
        //std::cout << "GET: waiting" << std::endl;

        _cv.wait(_write_mutex, [this, &result, &success](){ return (success = _queue.get(result)) || _closed; });
        //std::cout  << "GET: resuming" << std::endl;
        _write_mutex.unlock();
    }

    if (!success)
    {
        assert(_closed);
        throw channel_closed();
    }

    //std::cout << "GET: q size after get: " << _queue.size() << "/" << _queue.capacity() << std::endl;
    if (_queue.size() == _queue.capacity() - 2)
    {
         //std::cout << "GET: waking up producer" << std::endl;
        _cv.notify_all();
    }
    return result;
}

template<typename T, typename ConditionVariable>
void threaded_channel<T, ConditionVariable>::writer_close()
{
    std::lock_guard<mutex> lock(_write_mutex);
    do_close();
}

template<typename T, typename ConditionVariable>
void threaded_channel<T, ConditionVariable>::reader_close()
{
    std::lock_guard<mutex> lock(_read_mutex);
    do_close();
}

template<typename T, typename ConditionVariable>
void threaded_channel<T, ConditionVariable>::do_close()
{
    if (!_closed)
    {
        _closed = true;
        _cv.notify_all();
    }

}

}

#endif
