#include "Logger.h"

// Static variables
Logger* Logger::log = nullptr;
long long Logger::count = 0;
long long Logger::mod = 1e18;
long long Logger::index = 0;
vector<ClientTranscationInstance> Logger::rec;
mutex Logger::mtx;
condition_variable Logger :: isEmpty;
string DecoratorForTranSaction::backendTranscationKey = "DEFAULT_KEY";

// Logger methods
Logger::Logger() {}

string Logger::TokenGenerator() {
    auto now = chrono::system_clock::now();
    long long timeNow = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    count++;
    count = count % mod;

    string Token = to_string(timeNow) + to_string(count);
    return Token;
}

// this was updated later on : so test it properly.
Logger& Logger::getInstance() {
        static Logger instance; 
        return instance;
}

void Logger::push(tempTransaction t) {
    unique_lock<mutex> ul(mtx);
    string token = TokenGenerator();
    t.setTranscationId(token);
    ClientTranscationInstance temp = DecoratorForTranSaction::decorate(t);
    rec.push_back(temp);
    Logger :: isEmpty.notify_one();
}

// tempTransaction
tempTransaction::tempTransaction() {
    this->TranscationId = "NULL";
    this->phoneNumber = "NULL";
    this->name = "NULL";
    this->modeOfPayement = "NULL";
    this->amount = 0.0;
}

void tempTransaction::setTranscationId(string token) {
    this->TranscationId = token;
}
void tempTransaction::setPhoneNumber(string phoneNumber) {
    this->phoneNumber = phoneNumber;
}
void tempTransaction::setName(string name) {
    this->name = name;
}
void tempTransaction::setModeOfPayement(string modeOfPayement) {
    this->modeOfPayement = modeOfPayement;
}
void tempTransaction::setAmount(float amount) {
    this->amount = amount;
}
string tempTransaction::getTansactionId() {
    return this->TranscationId;
}

// ClientTranscationInstance
ClientTranscationInstance::ClientTranscationInstance(tempTransaction& t) {
    this->amount = t.amount;
    this->name = t.name;
    this->phoneNumber = t.phoneNumber;
    this->TranscationId = t.getTansactionId();
    this->backendTranscationKey = "NAN";
}
void ClientTranscationInstance::setbackendTranscationKey(string backendTranscationKey) {
    this->backendTranscationKey = backendTranscationKey;
}
string ClientTranscationInstance::getTansactionId() {
    return this->TranscationId;
}
string ClientTranscationInstance::getbackendTranscationKey() {
    return this->backendTranscationKey;
}

// DecoratorForTranSaction
ClientTranscationInstance DecoratorForTranSaction::decorate(tempTransaction& t) {
    ClientTranscationInstance temp(t);
    temp.setbackendTranscationKey(backendTranscationKey);
    return temp;
}
