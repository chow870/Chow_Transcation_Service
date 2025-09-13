#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <mutex>
#include <vector>
#include <chrono>
#include <string>
#include <condition_variable>
using namespace std;

// Forward declarations to avoid circular dependency
class tempTransaction;
class ClientTranscationInstance;

class Logger {
private:
    static vector<ClientTranscationInstance> rec;
    static mutex mtx;
    
    static long long count;
    static long long mod;
    static Logger* log;
    static long long index;

    Logger(); // private constructor (singleton)
    Logger(const  Logger&) = delete;
    Logger& operator=(const  Logger&) = delete;
    static string TokenGenerator();

public:
    static condition_variable isEmpty;
    static Logger& getInstance(); // get singleton instance
    static void push(tempTransaction t);
    static size_t getSize(){
        // unique_lock<mutex>lock(mtx);
        return rec.size();
    }
    static ClientTranscationInstance & pop(){
        cout<<"reached the pop of logger "<<'\n';
        unique_lock<mutex>lock(mtx);
        cout<<"the lock is accessible to me in pop "<<'\n';
        cout<<"the logger size is : "<<Logger::getSize() <<" the index is : "<<Logger::index<<'\n';
        isEmpty.wait(lock, []{ return (((int) Logger::getSize()) > index+1);} ); // means if not empty come on
        cout<<"i was able to pop out something"<<endl;
        return rec[index++]; // index incremented
    }
};

class tempTransaction {
private:
    string TranscationId;
public:
    string phoneNumber;
    string name;
    string modeOfPayement;
    float amount;

    tempTransaction();
    void setTranscationId(string token);
    void setPhoneNumber(string phoneNumber);
    void setName(string name);
    void setModeOfPayement(string modeOfPayement);
    void setAmount(float amount);
  
    string getTansactionId();
};

class ClientTranscationInstance {
private:
    string backendTranscationKey;
    string TranscationId;

public:
    string phoneNumber;
    string name;
    string modeOfPayement;
    float amount;

    ClientTranscationInstance(tempTransaction& t);
    void setbackendTranscationKey(string backendTranscationKey);

    string getTansactionId();
    string getbackendTranscationKey();
};

class DecoratorForTranSaction {
private:
    static string backendTranscationKey;

public:
    static ClientTranscationInstance decorate(tempTransaction& t);
};

#endif
