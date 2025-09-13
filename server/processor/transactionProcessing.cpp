#include<iostream>
#include "Logger.h"
#include "TransactionQueue.h"
#include<mutex>
#include<thread>
#include<condition_variable>
using namespace std;


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
        return processor->makePayement(amount);
    }

};


class TransactionStrategy{
    public : 
    virtual bool makePayement(float amount) =0;
    virtual ~TransactionStrategy(){};
};

class CreditCardStrategy : public TransactionStrategy{

    public : 
    CreditCardStrategy(){
        cout<<"made an instance of the credit card "<<endl;
    }

    bool makePayement(float amount){
        // do some processing here
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
        // do some processing here
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


    // this will be inisiated with the help of the 
    void handlePayement(){
        ClientTranscationInstance temp = Dequeue->pop();
        // abhi file mai log bhi linkha hai yaha par.
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
        // file mai log likhna parega isme bhi. 
    }



};