// unified_with_graceful_shutdown.cpp
// Minimal logic changes: added graceful shutdown coordination, counters, and thread exception guards.

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <deque>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <random>
#include <fstream>

using namespace std;

// -----------------------------
// Global counters for coordination
// -----------------------------
static atomic<long long> g_submittedCount{0};   // CHANGED: increment when Submit() accepts
static atomic<long long> g_processedCount{0};   // CHANGED: increment when consumer finishes processing

// -----------------------------
// Small utilities for logging
// -----------------------------
enum class ErrorType {
    None,
    RateLimitExceeded,
    BackpressureDrop,
    BatchUploadFailed,
    CircuitOpen,
    GatewayTimeout,
    GatewayFailure,
    PermanentFailure,
    Unknown
};

static string errorTypeToString(ErrorType t) {
    switch (t) {
        case ErrorType::None: return "None";
        case ErrorType::RateLimitExceeded: return "RateLimitExceeded";
        case ErrorType::BackpressureDrop: return "BackpressureDrop";
        case ErrorType::BatchUploadFailed: return "BatchUploadFailed";
        case ErrorType::CircuitOpen: return "CircuitOpen";
        case ErrorType::GatewayTimeout: return "GatewayTimeout";
        case ErrorType::GatewayFailure: return "GatewayFailure";
        case ErrorType::PermanentFailure: return "PermanentFailure";
        default: return "Unknown";
    }
}

// A single log entry representing the state at a stage (input/mid/processor/consumer)
struct LogEntry {
    string stage;            // e.g., "input_shard", "midstream_queue", "batch_upload", "consumer_queue", "processor"
    string transactionId;
    bool success;
    string errType;         // textual error type
    string message;         // additional message
    double timestamp;       // seconds since epoch steady_clock
    int queueSize;          // size of relevant queue after action (if applicable)

    LogEntry() = default;
    LogEntry(const string& stage,
             const string& txnId,
             bool success,
             const string& errType,
             const string& message,
             int queueSize)
        : stage(stage),
          transactionId(txnId),
          success(success),
          errType(errType),
          message(message),
          timestamp(chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()),
          queueSize(queueSize) {}
};

class Logger {
public:
    static mutex m;
    static vector<LogEntry> entries;

    // Thread-safe push of a log entry
    static void log(const string& stage,
                    const string& txnId,
                    bool success,
                    const string& errType,
                    const string& message,
                    int queueSize = -1) {
        lock_guard<mutex> lg(m);
        entries.emplace_back(stage, txnId, success, errType, message, queueSize);
    }

    // Dump as CSV for visualization later
    static void dumpToFile(const string& filename) {
        lock_guard<mutex> lg(m);
        ofstream out(filename);
        if (!out.is_open()) return;
        out << "stage,transactionId,success,errType,message,timestamp,queueSize\n";
        for (auto &e : entries) {
            out << e.stage << "," << e.transactionId << "," << (e.success ? "1" : "0") << ",";
            out << "\"" << e.errType << "\"" << "," << "\"" << e.message << "\"" << ",";
            out << e.timestamp << "," << e.queueSize << "\n";
        }
        out.close();
    }
};

mutex Logger::m;
vector<LogEntry> Logger::entries;

// Forward declarations
class ConsumerWorkerPool;
class ProducerShard;

// -----------------------------
// Domain objects
// -----------------------------
class InitialTransaction {
private:
    string transactionId;
public:
    string phoneNumber="9887873879379";
    long long clientID = 0;
    string name="Aditya";
    string modeOfPayment;
    float amount = 0.0f;

    InitialTransaction() = default;

    void setTranscationId(const string& token){ this->transactionId = token; }
    void setPhoneNumber(const string& phoneNumber){ this->phoneNumber = phoneNumber; }
    void setName(const string& name){ this->name = name; }
    void setModeOfPayment(const string& modeOfPayment){ this->modeOfPayment = modeOfPayment; }
    void setAmount(float amount){ this->amount = amount; }
    string getTansactionId() const { return transactionId; }
    long long int clientId() const { return clientID; }
};

// Encrypted transaction (stub)
class EncrpytedTransaction {
public:
    string modeOfPayment;
    float amount = 0.0f;
    string transactionId;
};

// -----------------------------
// Base thread-pool abstraction
// -----------------------------
class WorkerThreadPool {
protected:
    int poolSize = 0;
    vector<thread> workers;
    mutex taskMutex;
    atomic<bool>shutdown{false};

public:
    condition_variable taskCV;
    WorkerThreadPool(int size = 0) : poolSize(size) {}

    virtual ~WorkerThreadPool() {
        // CHANGED: set shutdown and notify, then join threads
        shutdown.store(true);
        taskCV.notify_all();
        for (auto &t : workers) {
            if (t.joinable()) t.join();
        }
    }

    virtual void submit() = 0;
};

// -----------------------------
// Token bucket (thread-safe)
// -----------------------------
class TokenBucket {
    double tokens;
    double capacity;
    double refillRate;
    chrono::steady_clock::time_point lastRefill;
    mutex mtx;

public:
    TokenBucket(double rate = 5.0, double cap = 20.0)
        : tokens(cap), capacity(cap), refillRate(rate), lastRefill(chrono::steady_clock::now()) {}

    bool allowRequest() {
        lock_guard<mutex> lock(mtx);
        auto now = chrono::steady_clock::now();
        double seconds = chrono::duration<double>(now - lastRefill).count();
        tokens = min(capacity, tokens + seconds * refillRate);
        lastRefill = now;
        if (tokens >= 1.0) {
            tokens -= 1.0;
            return true;
        }
        return false;
    }
};

// -----------------------------
// ProducerShard - per-shard input queue
// -----------------------------
class ProducerShard {
private:
    deque<InitialTransaction> rec;
    int MaxQueueSize = 100;
    WorkerThreadPool * pool;
    TokenBucket * rps;
    mutex m;
public:
    ProducerShard(WorkerThreadPool * pool) {
        this->pool = pool;
        rps = new TokenBucket();
    }

    ~ProducerShard() {
        delete rps;
    }

    // popTask now blocks until an entry exists
    InitialTransaction popTask() {
        unique_lock<mutex> lock(m);
        while (rec.empty()) {
            lock.unlock();
            this_thread::sleep_for(chrono::milliseconds(1));
            lock.lock();
        }
        InitialTransaction t = rec.front();
        rec.pop_front();
        return t;
    }

    bool Isempty() {
        lock_guard<mutex> lock(m);
        return rec.empty();
    }

    // Submit a transaction to the shard
    bool Submit(InitialTransaction & obj) {
        unique_lock<mutex> lock(m);
        if (!rps->allowRequest()) {
            Logger::log("input_shard", obj.getTansactionId(), false, errorTypeToString(ErrorType::RateLimitExceeded),
                        "rate limited at shard", (int)rec.size());
            cout << obj.getTansactionId() << " Producer Shard rate limited\n";
            return false;
        }
        if (rec.size() >= (size_t)MaxQueueSize) {
            Logger::log("input_shard", obj.getTansactionId(), false, errorTypeToString(ErrorType::BackpressureDrop),
                        "backpressure drop at shard", (int)rec.size());
            cout << obj.getTansactionId() << " Producer Shard backPressure\n";
            return false;
        }
        rec.push_back(obj);
        int qsize = (int)rec.size();
        lock.unlock();

        // LOG: input stage success
        Logger::log("input_shard", obj.getTansactionId(), true, errorTypeToString(ErrorType::None),
                    "accepted into shard", qsize);

        // CHANGED: increment global submitted counter
        g_submittedCount.fetch_add(1, std::memory_order_relaxed);

        // Notify pool that work is available
        if (pool) pool->taskCV.notify_all();
        return true;
    }
};

// -----------------------------
// Circuit Breaker
// -----------------------------
class CircuitBreaker{
    enum class State { CLOSED, OPEN, HALF_OPEN };
    State state = State::CLOSED;
    int failureCount = 0;
    mutex m;
    const int failureThreshold = 3;
    chrono::steady_clock::time_point lastFailureTime;
    const chrono::seconds openDuration = chrono::seconds(1);

public:
    bool allowRequest() {
        lock_guard<mutex> gd(m);
        if (state == State::OPEN) {
            auto now = chrono::steady_clock::now();
            if (now - lastFailureTime > openDuration) {
                state = State::HALF_OPEN;
                return true;
            }
            return false;
        }
        return true;
    }

    void recordSuccess() {
        lock_guard<mutex> gd(m);
        failureCount = 0;
        state = State::CLOSED;
    }

    void recordFailure() {
        lock_guard<mutex> gd(m);
        failureCount++;
        if (failureCount >= failureThreshold) {
            state = State::OPEN;
            lastFailureTime = chrono::steady_clock::now();
        }
    }
};

// -----------------------------------------
// SUCCESS / ERROR / RESPONSE
// -----------------------------------------
class SuccessRec {
public:
    string transactionId;
    bool status = true;
    int retryCount = 0;
    chrono::steady_clock::time_point timestamp;

    SuccessRec(string transactionId, int retryCount) {
        this->retryCount = retryCount;
        this->transactionId = transactionId;
        this->timestamp = chrono::steady_clock::now();
    }
    SuccessRec() {}
};

class ErrorRecord {
public:
    string transactionId;
    ErrorType type = ErrorType::None;
    string message;
    int retryCount = 0;
    double timestamp;

    ErrorRecord(string transactionId, ErrorType type, string message, int retryCount) {
        this->transactionId = transactionId;
        this->type = type;
        this->message = message;
        this->retryCount = retryCount;
        this->timestamp = chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count();
    }
    ErrorRecord() {}

    void SetErrorMessage(const string& layer) {
        message = "Refer to the Error code. Error occurred in " + layer;
    }
};

class Response {
public:
    ErrorRecord rec1;
    SuccessRec rec2;
    bool status = false;

    Response() {}
    Response(ErrorRecord rec1, bool status, SuccessRec rec2) {
        this->rec1 = rec1;
        this->status = status;
        this->rec2 = rec2;
    }
};

// ======================================================================
// RANDOM FAILURE SIMULATION HELPERS
// ======================================================================
static ErrorType randomErrorType() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dist(1, 7);
    switch (dist(gen)) {
        case 1: return ErrorType::RateLimitExceeded;
        case 2: return ErrorType::BackpressureDrop;
        case 3: return ErrorType::BatchUploadFailed;
        case 4: return ErrorType::CircuitOpen;
        case 5: return ErrorType::GatewayTimeout;
        case 6: return ErrorType::GatewayFailure;
        case 7: return ErrorType::PermanentFailure;
        default: return ErrorType::Unknown;
    }
}

static bool shouldFail(double failureRate = 0.3) {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<> dist(0.0, 1.0);
    return dist(gen) < failureRate;
}

// ======================================================================
// STRATEGY PATTERN BASE & IMPLEMENTATIONS
// ======================================================================
class TransactionStrategy {
public:
    virtual Response makePayement(EncrpytedTransaction & temp) = 0;
    virtual ~TransactionStrategy() {}
};

class CreditCardStrategy : public TransactionStrategy {
private:
    int latency = 10; // ms
public:
    CreditCardStrategy(){}

    Response makePayement(EncrpytedTransaction & temp) override {
        this_thread::sleep_for(chrono::milliseconds(latency));
        string txnId = temp.transactionId;
        if (shouldFail(0.3)) {
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "Credit Card transaction failed", 0);
            rec.SetErrorMessage("CreditCardStrategy");
            Logger::log("processor", txnId, false, errorTypeToString(err), rec.message, -1);
            return Response(rec, false, SuccessRec());
        }
        SuccessRec success(txnId, 0);
        Logger::log("processor", txnId, true, errorTypeToString(ErrorType::None), "processed by CreditCardStrategy", -1);
        return Response(ErrorRecord(), true, success);
    }

    void setLatency(int l) { this->latency = l; }
};

class UPIStrategy : public TransactionStrategy {
private:
    int latency = 10;
public:
    UPIStrategy() {}

    Response makePayement(EncrpytedTransaction & temp) override {
        this_thread::sleep_for(chrono::milliseconds(latency));
        string txnId = temp.transactionId;
        if (shouldFail(0.2)) {
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "UPI payment failed", 0);
            rec.SetErrorMessage("UPIStrategy");
            Logger::log("processor", txnId, false, errorTypeToString(err), rec.message, -1);
            return Response(rec, false, SuccessRec());
        }
        SuccessRec success(txnId, 0);
        Logger::log("processor", txnId, true, errorTypeToString(ErrorType::None), "processed by UPIStrategy", -1);
        return Response(ErrorRecord(), true, success);
    }

    void setLatency(int l) { this->latency = l; }
};

class NetBankingStrategy : public TransactionStrategy {
private:
    int latency = 70;
public:
    NetBankingStrategy() {}

    Response makePayement(EncrpytedTransaction & temp) override {
        this_thread::sleep_for(chrono::milliseconds(latency));
        string txnId = temp.transactionId;
        if (shouldFail(0.4)) {
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "NetBanking payment failed", 0);
            rec.SetErrorMessage("NetBankingStrategy");
            Logger::log("processor", txnId, false, errorTypeToString(err), rec.message, -1);
            return Response(rec, false, SuccessRec());
        }
        SuccessRec success(txnId, 0);
        Logger::log("processor", txnId, true, errorTypeToString(ErrorType::None), "processed by NetBankingStrategy", -1);
        return Response(ErrorRecord(), true, success);
    }

    void setLatency(int l) { this->latency = l; }
};

// ======================================================================
// PROCESSOR + HANDLER LAYER
// ======================================================================
class TransactionProcessor {
private:
    TransactionStrategy* processor = nullptr;
public:
    TransactionProcessor() = default;

    void setTransactionStrategy(TransactionStrategy* t) {
        if (processor) delete processor;
        processor = t;
    }

    Response makePayement(EncrpytedTransaction & temp) {
        if (processor != nullptr)
            return processor->makePayement(temp);
        ErrorRecord rec("UNKNOWN", ErrorType::Unknown, "No processor set", 0);
        rec.SetErrorMessage("TransactionProcessor");
        Logger::log("processor", rec.transactionId, false, errorTypeToString(ErrorType::Unknown), rec.message, -1);
        return Response(rec, false, SuccessRec());
    }

    ~TransactionProcessor() {
        if (processor) delete processor;
    }
};

// ======================================================================
// HANDLER API
// ======================================================================
class TransactionHandlerAPI {
private:
    TransactionProcessor* processor;
public:
    TransactionHandlerAPI(TransactionProcessor* t) : processor(t) {}

    Response handlePayement(EncrpytedTransaction& temp) {
        if (temp.modeOfPayment == "UPI") {
            processor->setTransactionStrategy(new UPIStrategy());
        } else if (temp.modeOfPayment == "CreditCard") {
            processor->setTransactionStrategy(new CreditCardStrategy());
        } else {
            processor->setTransactionStrategy(new NetBankingStrategy());
        }

        Logger::log("processor_enter", temp.transactionId, true, errorTypeToString(ErrorType::None), "entered processor", -1);
        Response r = processor->makePayement(temp);
        if (r.status) {
            Logger::log("processor_exit", r.rec2.transactionId, true, errorTypeToString(ErrorType::None), "processor exit success", -1);
        } else {
            Logger::log("processor_exit", r.rec1.transactionId, false, errorTypeToString(r.rec1.type), r.rec1.message, -1);
        }
        return r;
    }
};

// -----------------------------
// ConsumerWorkerPool
// -----------------------------
class ConsumerWorkerPool : public WorkerThreadPool {
private:
    deque<EncrpytedTransaction> ActualTransactionQueue;
    TransactionHandlerAPI *gateway = nullptr;
    int MaxActualTQSize = 100;
    mutex m;
    atomic<bool> isWriting{false};
    condition_variable ConsumerCV;
public:
    CircuitBreaker * cb = nullptr;

    ConsumerWorkerPool(int size, TransactionHandlerAPI *gateway, CircuitBreaker *breaker = nullptr)
        : WorkerThreadPool(size), gateway(gateway), cb(breaker) {
        if (poolSize <= 0) poolSize = size;
        for (int i = 0; i < poolSize; ++i) {
            // CHANGED: add exception guard in thread lambda
            workers.emplace_back([this] {
                try {
                    this->workerPool();
                } catch (const std::exception& e) {
                    cerr << "[Consumer thread exception] " << e.what() << endl;
                } catch (...) {
                    cerr << "[Consumer thread unknown exception]" << endl;
                }
            });
        }
    }

    void workerPool() {
        while (!shutdown.load()) {
            if (cb && !cb->allowRequest()) {
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }

            unique_lock<mutex> lck(m);
            ConsumerCV.wait(lck,[this] {
                return !ActualTransactionQueue.empty() && !isWriting.load();
            });

            if (shutdown.load()) break;
            if (ActualTransactionQueue.empty()) continue;

            EncrpytedTransaction temp = ActualTransactionQueue.front();
            cout<<"processing : "<<temp.transactionId<<endl;
            ActualTransactionQueue.pop_front();
            int qsize = (int)ActualTransactionQueue.size();
            // here i can add db.entry(TransactionId) = "pending"
            ConsumerCV.notify_all();
            lck.unlock();

            // here you see that i have started processing after the lock is left.
            // LOG: consumer popped the item
            Logger::log("consumer_queue_pop", temp.transactionId, true, errorTypeToString(ErrorType::None),
                        "popped for processing", qsize);

            // process it (call gateway)
            Response resp = gateway->handlePayement(temp);
            
            // CHANGED: increment processed counter whether success or failure (we reached processing finish)
            g_processedCount.fetch_add(1, std::memory_order_relaxed);

            if (resp.status) {
                if (cb) cb->recordSuccess();
            } else {
                Logger::log("processor_result", resp.rec1.transactionId, false, errorTypeToString(resp.rec1.type),
                            resp.rec1.message, qsize);
                if (resp.rec1.type == ErrorType::GatewayFailure || resp.rec1.type == ErrorType::GatewayTimeout) {
                    if (cb) cb->recordFailure();
                    // retry logic would be added later
                } else {
                    // permanent / unknown failure - drop after logging
                }
            }
        }
    }
    

    void UploadFromTheUpstream(vector<EncrpytedTransaction> batch) {
        cout<<"started waiting for the batch upload in the UploadFromTheUpstream"<<endl;
        isWriting.store(true);

        unique_lock<mutex> lck(m);
        ConsumerCV.wait(lck,[this] {
            return (int)ActualTransactionQueue.size() < MaxActualTQSize || shutdown.load();
        });

        // LOG: batch upload event
        if (!batch.empty()) {
            Logger::log("batch_upload", batch.front().transactionId, true, errorTypeToString(ErrorType::None),
                        "attempting batch upload", (int)ActualTransactionQueue.size());
        }
        cout<<"batch upload started"<<endl;

        for (auto &itr : batch) {
            if ((int)ActualTransactionQueue.size() < MaxActualTQSize) {
                ActualTransactionQueue.push_back(itr);
                Logger::log("consumer_queue_add", itr.transactionId, true, errorTypeToString(ErrorType::None),
                            "added to consumer queue", (int)ActualTransactionQueue.size());
            } else {
                Logger::log("consumer_queue_drop", itr.transactionId, false, errorTypeToString(ErrorType::BatchUploadFailed),
                            "dropped during batch upload (consumer full)", (int)ActualTransactionQueue.size());
                cout << itr.transactionId << " happened batch upload (dropped)\n";
                // We DO NOT increment g_processedCount for dropped ones
            }
        }

        isWriting.store(false);
        lck.unlock();
        ConsumerCV.notify_all();
    }

    bool submitOne(const EncrpytedTransaction &t) {
        unique_lock<mutex> lck(m);
        if ((int)ActualTransactionQueue.size() >= MaxActualTQSize) {
            Logger::log("consumer_queue_submit", t.transactionId, false, errorTypeToString(ErrorType::BackpressureDrop),
                        "submitOne rejected (consumer full)", (int)ActualTransactionQueue.size());
            return false;
        }
        ActualTransactionQueue.push_back(t);
        int qsize = (int)ActualTransactionQueue.size();
        lck.unlock();
        ConsumerCV.notify_all();
        Logger::log("consumer_queue_submit", t.transactionId, true, errorTypeToString(ErrorType::None),
                    "submitOne success", qsize);
        return true;
    }

    void submit() override {}
};

// -----------------------------
// ProducerWorkerPool
// -----------------------------
class ProducerWorkerPool : public WorkerThreadPool {
private:
    deque<EncrpytedTransaction> batchUploadQueue;
    thread batchUploader;
    int MaxSizeOfMidstreamQueue;
    chrono::steady_clock::time_point lastUpload;
    ConsumerWorkerPool * CWorkerPool = nullptr;
    double BatchTime = 2.0;
    atomic<bool> Uploading{false};
    int BatchSize = 10;

public:
    ProducerWorkerPool(int size, ConsumerWorkerPool * CWorkerPool, ProducerShard & shard,int MaxSizeOfMidstreamQueue = 10 )
        : WorkerThreadPool(size), CWorkerPool(CWorkerPool) {
        if (poolSize <= 0) poolSize = size;
        this->MaxSizeOfMidstreamQueue = MaxSizeOfMidstreamQueue;
        for (int i = 0; i < poolSize; ++i) {
            workers.emplace_back([this, &shard] {
                try {
                    this->workerPool(shard);
                } catch (const std::exception& e) {
                    cerr << "[Producer thread exception] " << e.what() << endl;
                } catch (...) {
                    cerr << "[Producer thread unknown exception]" << endl;
                }
            });
        }
        lastUpload = chrono::steady_clock::now();
        batchUploader = thread(&ProducerWorkerPool::BatchUploadCheck, this);
    }

    ~ProducerWorkerPool() {
        shutdown.store(true);
        taskCV.notify_all();
        if (batchUploader.joinable()) batchUploader.join();
        // WorkerThreadPool destructor will join producer worker threads
    }

    EncrpytedTransaction encryptTransaction(InitialTransaction &it) {
        EncrpytedTransaction et;
        et.modeOfPayment = it.modeOfPayment;
        et.amount = it.amount;
        et.transactionId = it.getTansactionId();
        return et;
    }

    void workerPool(ProducerShard & shard) {
        while (!shutdown.load()) {
            cout<<"producer thread waiting to acquire the mutex "<<endl;
            unique_lock<mutex> lock(taskMutex);
            taskCV.wait_for(lock, chrono::milliseconds(200), [&] {
                return (!shard.Isempty() && !Uploading.load()) || shutdown.load();
            });

            if (shutdown.load()) break;

            if (batchUploadQueue.size() >= (size_t)MaxSizeOfMidstreamQueue) {
                lock.unlock();
                this_thread::sleep_for(chrono::milliseconds(30));
                continue;
            }

            if (shard.Isempty()) {
                continue;
            }

            cout<<"producer thread acquired the mutex"<<endl;
            InitialTransaction temp = shard.popTask();
            EncrpytedTransaction et = encryptTransaction(temp);
            cout<<"the producer thread has "<<et.transactionId<<endl;

            batchUploadQueue.push_back(std::move(et));
            int qsize = (int)batchUploadQueue.size();

            // LOG: midstream queue state (producer pushed into midstream queue)
            Logger::log("midstream_queue", batchUploadQueue.back().transactionId, true, errorTypeToString(ErrorType::None),
                        "pushed to midstream queue", qsize);

            lock.unlock();
            taskCV.notify_all();
        }
    }

    void BatchUploadCheck() {
        while (!shutdown.load()) {
            this_thread::sleep_for(chrono::milliseconds(50));

            auto elapsed = chrono::duration_cast<chrono::duration<double>>(chrono::steady_clock::now() - lastUpload).count();
            bool shouldUpload = (elapsed > BatchTime) || ((int)batchUploadQueue.size() >= BatchSize);
            if (!shouldUpload) continue;
            cout<<"batch uploader wants to uplaod now in producer side "<<endl;
            if (!CWorkerPool->cb->allowRequest()) {
                continue;
            }
            cout<<"batch uploader cb.allowed now in producer side "<<endl;
            vector<EncrpytedTransaction> batch;
            {   Uploading.store(true);
                unique_lock<mutex> lock(taskMutex); // is this source of deadlock ? 
                cout<<"began making the batch size"<<endl;
                int tempBatchSize = BatchSize;
                while (tempBatchSize > 0 && !batchUploadQueue.empty()) {
                    batch.push_back(std::move(batchUploadQueue.front()));
                    batchUploadQueue.pop_front();
                    --tempBatchSize;
                }
                Uploading.store(false);
            }

            if (!batch.empty()) {
                Logger::log("batch_upload", batch.front().transactionId, true, errorTypeToString(ErrorType::None),
                            "batch created", (int)batch.size());
            }

            cout<<"going for the batch upload from produer side"<<endl;
            if (!batch.empty() && CWorkerPool) {
                CWorkerPool->UploadFromTheUpstream(batch);
            }
            cout<<"finally batch uploader came back in producer again"<<endl;
            lastUpload = chrono::steady_clock::now();
            taskCV.notify_all(); // idhar might be the issue. like agar vo deadloack mai fas gaya them batchuploader worker thread ko fasa dengi
        }
    }

    void submit() override {}
};

// -----------------------------
// main() with graceful drain/wait
// -----------------------------
int main() {
    TransactionProcessor proc;
    TransactionHandlerAPI gateway(&proc);
    CircuitBreaker cb1;
    CircuitBreaker cb2;

    ConsumerWorkerPool consumerPool1(2, &gateway, &cb1);
    ConsumerWorkerPool consumerPool2(2, &gateway, &cb2);

    ProducerShard shard1(&consumerPool1);
    ProducerShard shard2(&consumerPool2);
    ProducerWorkerPool producerPool1(2, &consumerPool1, shard1);
    ProducerWorkerPool producerPool2(2, &consumerPool2, shard2);

    long long submitted = 0;
    long long failed = 0;
    const int TOTAL_TO_SUBMIT = 3000;
    try{
         for (int i = 0; i < TOTAL_TO_SUBMIT; ++i) {
        InitialTransaction t1;
        string temp = "tx" + to_string(submitted);
        t1.setTranscationId(temp);
        t1.setModeOfPayment((submitted % 3 == 0) ? "UPI" : (submitted % 3 == 1) ? "CreditCard" : "NetBanking");
        t1.setAmount(10.0f + (float)(submitted % 100));
        submitted++;
        bool ok;
        if (submitted % 2) ok = shard1.Submit(t1);
        else ok = shard2.Submit(t1);

        if (!ok) { failed++; cout << "Submit t1 failed (rate limit/backpressure)\n"; }
        this_thread::sleep_for(chrono::milliseconds(10));
        cout << "total submitted " << submitted << " failed : " << failed << "\n";
    }

    // // Wait for work to drain with timeout (CHANGED):
    // auto startWait = chrono::steady_clock::now();
    // const auto MAX_WAIT = chrono::seconds(10);
    // while (chrono::steady_clock::now() - startWait < MAX_WAIT) {
    //     long long submittedNow = g_submittedCount.load(std::memory_order_relaxed);
    //     long long processedNow = g_processedCount.load(std::memory_order_relaxed);
    //     cout << "[drain] submitted=" << submittedNow << " processed=" << processedNow << "\n";
    //     if (submittedNow > 0 && processedNow >= submittedNow) {
    //         cout << "All submitted transactions processed. Proceeding to shutdown.\n";
    //         break;
    //     }
    //     this_thread::sleep_for(chrono::milliseconds(200));
    // }

    // Allow pools to destruct (their destructors set shutdown and join threads)
    // Dump logs
    Logger::dumpToFile("system_logs.csv");
    cout << "Logs dumped to system_logs.csv (" << Logger::entries.size() << " entries)\n";

    }
    catch(exception e){
        cerr<<e.what()<<endl;
    }

   

    return 0;
}


// TODO: Retry and Idempotency is still missing. 