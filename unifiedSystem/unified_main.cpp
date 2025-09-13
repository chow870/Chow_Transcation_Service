// ============================================================================
// UNIFIED TRANSACTION SYSTEM - ALL COMPONENTS IN ONE FILE
// This file combines:
// 1. client/AggressiveTestUser.cpp (Client Thread)
// 2. server/processor/initalProcessor.cpp (Processor Thread) 
// 3. server/processor/transactionProcessing.cpp (Handler Thread)
// ============================================================================

#include<iostream>
#include<mutex>
#include<thread>
#include<time.h>
#include<chrono>  // ADDED: For better time handling
#include<condition_variable>
#include "Logger.h"
#include "TransactionQueue.h"

using namespace std;

// ============================================================================
// SECTION 1: STRATEGY PATTERN CLASSES (from transactionProcessing.cpp)
// ============================================================================

// Forward declaration (ORIGINAL CODE - NO CHANGES)
class TransactionStrategy;

class TransactionStrategy{
    public : 
    virtual bool makePayement(float amount) =0;
    virtual ~TransactionStrategy(){};
};

class TransactionProcessor{
    private : 
    TransactionStrategy * processor;
    public : 
    TransactionProcessor(){
        processor = nullptr;
    }
    void setTransactionStrategy (TransactionStrategy * t){
        processor = t;
    }
    bool makePayement(float amount){
        // ADDED: Null check for safety
        if(processor != nullptr) {
            return processor->makePayement(amount);
        }
        return false;
    }
};

class CreditCardStrategy : public TransactionStrategy{
    public : 
    CreditCardStrategy(){
        cout<<"made an instance of the credit card "<<endl;
    }

    bool makePayement(float amount){
        // do some processing here (ORIGINAL COMMENT)
        this_thread::sleep_for(chrono::microseconds(10));
        return true;
    }
};

class UPIStrategy : public TransactionStrategy{
    public : 
    UPIStrategy(){
        cout<<"made an instance of the UPIStrategy"<<endl;
    }

    bool makePayement(float amount){
        // do some processing here (ORIGINAL COMMENT)
        this_thread::sleep_for(chrono::microseconds(10));
        return true;
    }
};

class NetBankingStrategy : public TransactionStrategy{
    public : 
    NetBankingStrategy(){
        cout<<"made an instance of the NetBankingStrategy"<<endl;
    }

    bool makePayement(float amount){
        // do some processing here 
        this_thread::sleep_for(chrono::microseconds(10));
        return true;  // ADDED: Missing return statement
    }
};

// there is a catch, agar aak TransactionHandler banaya and multple processor set kiya to performance bottle neck aayega
// thus each thread ko mai individual handler dunga 
class TransactionHandler{
    private : 
    TransactionProcessor processor;
    TransactionQueue* Dequeue;
    // here i will have the file pointer and access to it. 
    public : 
    TransactionHandler(TransactionProcessor & t){
        processor = t;
        Dequeue = &TransactionQueue::getInstance();
    }

    // ADDED: Default constructor to work with threading
    TransactionHandler(){
        Dequeue = &TransactionQueue::getInstance();
    }

    // this will be inisiated with the help of the (ORIGINAL COMMENT)
    void handlePayement(){
        ClientTranscationInstance temp = Dequeue->pop();
        // abhi file mai log bhi linkha hai yaha par. (ORIGINAL COMMENT)
        if(temp.modeOfPayement == "UPI"){
            processor.setTransactionStrategy(new UPIStrategy());
        }
        else if(temp.modeOfPayement == "CreditCard"){
            processor.setTransactionStrategy(new CreditCardStrategy());
        }
        else{
             processor.setTransactionStrategy(new NetBankingStrategy());
        }
        bool status = processor.makePayement(temp.amount);
        cout<<"the status is : "<<status<<endl; 
        // file mai log likhna parega isme bhi. 
    }

    // ADDED: Method to run the continuous processing loop
    void processLoop() {
        while(true){
            handlePayement();
            // this_thread::sleep_for(chrono::seconds(2));
        }
    }
};

// ============================================================================
// SECTION 2: INITIAL PROCESSOR CLASS (from initalProcessor.cpp)
// ============================================================================

// its main task is to create a deque type of data structure so that the worker or the processor thread can work on that
// currently my records are in the file system or in the vector 

class InitialProcessor{
   private:
    TransactionQueue* Dequeue;
    Logger* logger;
    // condition_variable isEmpty;  // ORIGINAL - keeping your condition_variable

public:
    InitialProcessor() {
        Dequeue = &TransactionQueue::getInstance();
        logger = &Logger::getInstance();
    }

    void push(){
        // i think i dont have to apply the mutex here beacuse individually the mutex are there in Dequeue and logger. (ORIGINAL COMMENT)
        Dequeue->push(logger->pop());
    }

    bool isEmpty(){
        cout<<"from the initalProcessor : size of the loger queue is : "<<logger->getSize()<<'\n';
        return (long long)logger->getSize() == (long long)0;
    }

    // ADDED: Method to run the continuous processing loop
    void processLoop() {
        while(1) {
            if (!isEmpty()) {
                push();
                // ADDED: Some feedback to see it working
                cout << "InitialProcessor: Moved transaction from Logger to Queue" << endl;
                cout << "Logger size: " << logger->getSize() << ", Queue size: " << Dequeue->size() << endl;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
};

// ============================================================================
// SECTION 3: AGGRESSIVE CLIENT (from AggressiveTestUser.cpp)
// ============================================================================

Logger * logger;  // ORIGINAL GLOBAL VARIABLE

void makeTransaction(){
    while(1){
    tempTransaction t;
    t.setAmount(234.3);
    t.setModeOfPayement("UPI");
    t.setName("Aditya");
    t.setPhoneNumber(
        "jdhkjfhdkhjkdshj");
    logger->push(t);
    Logger::isEmpty.notify_one();
    // ADDED: Feedback to see transactions being generated
    cout << "AggressiveClient: Generated transaction, Logger size: " << logger->getSize() << endl;
    this_thread::sleep_for(chrono :: seconds(2));
    }
}

// ============================================================================
// SECTION 4: ORIGINAL TASK FUNCTION (from transactionProcessing.cpp)
// ============================================================================

// void task(TransactionHandler & p){
//      while(true){
//         p.handlePayement();
//         this_thread::sleep_for(chrono::seconds(2));
//     }
// }

// ============================================================================
// SECTION 5: UNIFIED MAIN FUNCTION
// ============================================================================

int main(){
    cout << "============================================" << endl;
    cout << "UNIFIED TRANSACTION PROCESSING SYSTEM" << endl;
    cout << "Running all components in separate threads:" << endl;
    cout << "1. AggressiveTestUser (Client)" << endl;
    cout << "2. InitialProcessor (Logger->Queue)" << endl;  
    cout << "3. TransactionHandler (Queue->Processing)" << endl;
    cout << "============================================" << endl;

    // Initialize logger (ORIGINAL CODE from AggressiveTestUser.cpp)
    logger= &Logger::getInstance();

    // THREAD 1: Start Aggressive Client (ORIGINAL makeTransaction function)
    thread clientThread(makeTransaction);
    cout << "Started: AggressiveTestUser thread" << endl;

    // THREAD 2: Start Initial Processor
    InitialProcessor processor;
    thread processorThread([&processor]() {
        processor.processLoop();
    });
    cout << "Started: InitialProcessor thread" << endl;

    // THREAD 3: Start Transaction Handler
    // MODIFIED: Using your original approach but with proper threading
    TransactionHandler handler;
    thread handlerThread([&handler]() {
        handler.processLoop();
    });
    cout << "Started: TransactionHandler thread" << endl;

    cout << "\nAll threads started. System is now processing transactions..." << endl;

    // Join all threads (ORIGINAL pattern from your code)
    if(clientThread.joinable()) clientThread.join();
    if(processorThread.joinable()) processorThread.join();  
    if(handlerThread.joinable()) handlerThread.join();

    return 0;
}

// ============================================================================
// COMPILATION INSTRUCTIONS:
// g++ unified_main.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o unified_system
// 
// RUN INSTRUCTIONS:
// ./unified_system
// ============================================================================