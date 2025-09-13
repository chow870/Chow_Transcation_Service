#include<iostream>
#include<mutex>
#include<thread>
#include<time.h>
#include "Logger.h"

using namespace std;
Logger * logger;
void makeTransaction(){
    while(1){
    tempTransaction t;
    t.setAmount(234.3);
    t.setModeOfPayement("UPI");
    t.setName("Aditya");
    t.setPhoneNumber(
        "jdhkjfhdkhjkdshj");
    logger->push(t);
    this_thread::sleep_for(chrono :: seconds(2));
    }
   

}


int main(){
    logger= &Logger::getInstance();
    thread t(makeTransaction);
    if(t.joinable()) t.join();
}