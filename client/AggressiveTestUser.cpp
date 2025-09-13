#include<iostream>
#include<mutex>
#include<thread>
#include<time.h>
#include<chrono>
#include "Logger.h"

using namespace std;

Logger* logger;

void makeTransaction(){
    cout << "Transaction generator thread started..." << endl;
    int transactionCount = 0;
    
    while(1){
        tempTransaction t;
        t.setAmount(234.3 + (transactionCount % 100)); // Vary amount slightly
        t.setModeOfPayement("UPI");
        t.setName("Aditya");
        t.setPhoneNumber("jdhkjfhdkhjkdshj");
        
        logger->push(t);
        transactionCount++;
        
        cout << "Generated transaction #" << transactionCount 
             << " - Logger size: " << logger->getSize() << endl;
             
        this_thread::sleep_for(chrono::seconds(2));
    }
}

int main(){
    cout << "Starting Aggressive Test User (Client)..." << endl;
    
    logger = &Logger::getInstance();
    
    cout << "Initial Logger size: " << logger->getSize() << endl;
    
    thread transactionThread(makeTransaction);
    
    // Optional: Add a monitoring thread to show statistics
    thread monitorThread([]() {
        while(1) {
            this_thread::sleep_for(chrono::seconds(10));
            cout << "\n=== STATISTICS ===" << endl;
            cout << "Current Logger size: " << logger->getSize() << endl;
            cout << "==================\n" << endl;
        }
    });
    
    cout << "Client is running. Press Ctrl+C to stop." << endl;
    
    // Join the main transaction thread (runs indefinitely)
    if(transactionThread.joinable()) {
        transactionThread.join();
    }
    
    // The monitor thread will be terminated when main thread exits
    if(monitorThread.joinable()) {
        monitorThread.detach(); // Detach since it's just for monitoring
    }
    
    return 0;
}