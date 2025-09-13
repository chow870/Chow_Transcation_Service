#ifndef TRANSACTION_QUEUE_H
#define TRANSACTION_QUEUE_H

#include <iostream>
#include <deque>
#include <mutex>
#include <condition_variable>
#include "Logger.h"  

using namespace std;

// Thread-safe transaction queue hai
// 
class TransactionQueue {
private:
    deque<ClientTranscationInstance> myqueue;
    mutex mtx;
    condition_variable cv;
    TransactionQueue() {}
    TransactionQueue(const TransactionQueue&) = delete;
    TransactionQueue& operator=(const TransactionQueue&) = delete;

public:

    static TransactionQueue& getInstance() {
        static TransactionQueue instance; 
        return instance;
    }
   
    void push(ClientTranscationInstance t) {
        unique_lock<mutex> lock(mtx);
        myqueue.push_back(std::move(t));
        cv.notify_one();
    }


    ClientTranscationInstance pop() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return !myqueue.empty(); });

        ClientTranscationInstance t = myqueue.front();
        myqueue.pop_front();
        return t;
    }


    bool empty() {
        unique_lock<mutex> lock(mtx);
        return myqueue.empty();
    }


    size_t size() {
        unique_lock<mutex> lock(mtx);
        return myqueue.size();
    }
};

#endif
