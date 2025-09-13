#include<iostream>
#include "Logger.h"
#include "TransactionQueue.h"
#include<mutex>
#include<thread>
#include<condition_variable>
using namespace std;




// its main task is to create a deque type of data structure so that the worker or the processor thread can work on that
// currently my records are in the file system or in the vector

class InitialProcessor{
   private:
    TransactionQueue* Dequeue;
    Logger* logger;
    condition_variable isEmpty;


public:
    InitialProcessor() {
        Dequeue = &TransactionQueue::getInstance();
        logger = &Logger::getInstance();
    }

    void push(){
        // i think i dont have to apply the mutex here beacuse individually the mutex are there in Dequeue and logger.
        Dequeue->push(logger->pop());
    }

    // bool isEmpty(){
    //     return (long long)logger->getSize() == (long long) 0;
    // }
};

// later i can just start the threads to execute this.

void task(InitialProcessor & p){

    while(true){
        p.push();
        this_thread::sleep_for(chrono::seconds(2));
    }
}


int main(){

    // here i will be starting with the starting threads for this
    InitialProcessor p;
    thread t(task);

    if(t.joinable()) t.join();

}
