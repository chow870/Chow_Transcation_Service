#include<iostream>
#include<mutex>
#include<vector>
#include<chrono>
#include<string>
// #include <boost/uuid/uuid.hpp>
// #include <boost/uuid/uuid_generators.hpp>
// #include <boost/uuid/uuid_io.hpp>
using namespace std;


// this is a logger that will help me to to interact with the user using the shared file system. This can be avoided completely by using some source of 
// communication.


// this is a endPoint.
class Logger{
    private: 
    // here will be a file system 
    static vector<ClientTranscationInstance>rec;
    static mutex mtx;
    static long long count;
    static long long mod;
    Logger (){
       
    }

    static string TokenGenerator(){

        auto now = chrono::system_clock::now();
        long long timeNow= chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        count++;
        count = count%mod;

        string Token = to_string(timeNow) + to_string(count);
        return Token;

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
        string token = TokenGenerator();
        t.setTranscationId(token);
        ClientTranscationInstance temp = DecoratorForTranSaction ::decorate(t);
        rec.push_back(temp);
    }

};

Logger* Logger::log = nullptr;
long long Logger :: count = 0;
long long Logger :: mod = 1e18;


class tempTransaction{
    private : 
    string TranscationId;
    public : 
    string phoneNumber;
    string name;
    string modeOfPayement;
    float amount;
    
    // builder class design pattern for creating the instance fo class
    // this makes my code open for adding new features and constructor mai jayada parameters sai confusion hone lgta hai.

    tempTransaction(){
        this->TranscationId = "NULL";
        this->phoneNumber = "NULL";
        this->name = "NULL";
        this->modeOfPayement = "NULL";
        this->amount = 0.0;
    }

    void setTranscationId (string & token){
        this->TranscationId = token;
    }

    void setPhoneNumber (string & phoneNumber){
        this->phoneNumber = phoneNumber;
    }

    void setName(string & name){
        this->name = name;
    }

    void setModeOfPayement(string& modeOfPayement){
        this->modeOfPayement= modeOfPayement;
    }
    void setAmount(float amount){
        this->amount = amount;
    }
    
    string getTansactionId(){
        return this->TranscationId;
    }


};

class ClientTranscationInstance{
    private : 
    string backendTranscationKey; // this is to verfiy that maine or the authentic person nai hi kiya hai ye porcessing.
    // int version;
    string TranscationId;

    public : 
    string phoneNumber;
    string name;
    string modeOfPayement;
    float amount;


    ClientTranscationInstance(tempTransaction & t){
        this->amount = t.amount;
        this->name = t.name;
        this->phoneNumber = t.phoneNumber;
        this->TranscationId = t.getTansactionId();
        this->phoneNumber = t.phoneNumber;
        // this->version = -1;
        this->backendTranscationKey = "NAN";
    }

    void setbackendTranscationKey(string backendTranscationKey){
        this->backendTranscationKey = backendTranscationKey;
    }
    // void setVersionId(int version){
    //     this->version = version;
    // }

    string getTansactionId(){
        return this->TranscationId;
    }
    string getbackendTranscationKey (){
        return this->backendTranscationKey;
    } 
 

};

// this is just beacuse I wanted to separate these things or wanted to crate a wrapper.
// certainly i could have keep the backendKey in the logger but i thought that not actual task of logger, these things are additionally added on
// the top of the tempTranscationInstance so better to separate it. 

class DecoratorForTranSaction{
    private: 
    static string backendTranscationKey;
 

    public : 

    static ClientTranscationInstance& decorate(tempTransaction & t){
        ClientTranscationInstance temp(t);
        temp.setbackendTranscationKey(backendTranscationKey);
        return temp;
    }

};
