#include<iostream>
#include<mutex>
#include<vector>
using namespace std;


// this is a logger that will help me to to interact with the user using the shared file system. This can be avoided completely by using some source of 
// communication.


// this is a endPoint.
class Logger{
    private: 
    // here will be a file system 
    static vector<tempTransaction>rec;
    static mutex mtx;
    Logger (){
       
    }
    static Logger* log;
    public : 

    static Logger * insert (){
        if(log == nullptr){
            log = new Logger();
        }
        return log;
    }

    static void push(tempTransaction t){
        unique_lock<mutex> ul(mtx);
        rec.push_back(t);
    }

};

Logger* Logger::log = nullptr;


class tempTransaction{

};
