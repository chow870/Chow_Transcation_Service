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
    static condition_variable isEmpty;
    static long long count;
    static long long mod;
    static Logger* log;
    static long long index;

    Logger(); // private constructor (singleton)
    Logger(const  Logger&) = delete;
    Logger& operator=(const  Logger&) = delete;
    static string TokenGenerator();

public:
    static Logger& getInstance(); // get singleton instance
    static void push(tempTransaction t);
    static size_t getSize(){
        unique_lock<mutex>lock(mtx);
        return rec.size();
    }
    static ClientTranscationInstance & pop(){
        unique_lock<mutex>lock(mtx);
        isEmpty.wait(lock, []{ return (Logger::getSize() != (size_t)0);} ); // means if not empty come on
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
    void setTranscationId(string& token);
    void setPhoneNumber(string& phoneNumber);
    void setName(string& name);
    void setModeOfPayement(string& modeOfPayement);
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
