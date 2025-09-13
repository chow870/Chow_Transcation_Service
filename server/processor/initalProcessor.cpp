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
 
public:
    InitialProcessor() {
        Dequeue = &TransactionQueue::getInstance();
        logger  =  &Logger::getInstance();
    }

    void push(){
        // i think i dont have to apply the mutex here beacuse individually the mutex are there in Dequeue and logger.
        cout<<"entered the push of the initalProcessor"<<'\n';
        Dequeue->push(logger->pop());
    }

    bool isEmpty(){
        cout<<"the logger size is : "<<logger->getSize()<<'\n';
        return (long long)logger->getSize() == (long long) 0;
    }

    void processLoop() {
        cout << "InitialProcessor thread started..." << endl;
        while(1) {
            if (!isEmpty()) {
                push();
                cout << "Moved transaction from Logger to Queue" << endl;
                cout << "Logger size: " << logger->getSize() << ", Queue size: " << Dequeue->size() << endl;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
};

// later i can just start the threads to execute this.




int main(){

    // here i will be starting with the starting threads for this
   InitialProcessor processor;
    
    // Start processor thread using lambda
    thread processorThread([&processor]() {
        processor.processLoop();
    });
    
    // Let it run for a few seconds
    this_thread::sleep_for(chrono::seconds(5));
    if(processorThread.joinable()) processorThread.join();

}
