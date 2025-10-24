// fixed_and_completed.cpp
// Edited & annotated by assistant per user's request.
// NOTE: Uses raw pointers (per request). Be careful with ownership in production.

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

using namespace std;

// Forward declarations to resolve dependency order
class ConsumerWorkerPool;   // CHANGED: forward-declare to allow use in ProducerWorkerPool
class ProducerShard;       // CHANGED: forward-declare because ProducerWorkerPool references it
class ErrorRecord;


// -----------------------------
// Domain objects
// -----------------------------
class InitialTransaction {
private:
    string transactionId; // CHANGED: fixed name typo
public:
    string phoneNumber="9887873879379";
    long long clientID = 0;
    string name="Aditya";
    string modeOfPayment;
    float amount = 0.0f;

    InitialTransaction() = default;

    void setTranscationId(string& token){ this->transactionId = token; } // kept name method
    void setPhoneNumber(const string& phoneNumber){ this->phoneNumber = phoneNumber; }
    void setName(const string& name){ this->name = name; }
    void setModeOfPayment(const string& modeOfPayment){ this->modeOfPayment = modeOfPayment; }
    void setAmount(float amount){ this->amount = amount; }
    string getTansactionId(){ return transactionId; }
    long long int clientId() const { return clientID; }
};

// i will have to encrypt this and then send in the downstream queue.
class EncrpytedTransaction {
public:
    // CHANGED: add minimal fields used later
    string modeOfPayment;
    float amount = 0.0f;
    string transactionId;
    // TODO: add actual encrypted payload, metadata, IV, etc.
};

// -----------------------------
// Base thread-pool abstraction
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

class Logger {
    
    public : 
    static mutex m;
    static vector<ErrorRecord> rec;
    // static void log(ErrorRecord t){
    //     unique_lock<mutex>lck(m);
    //     cout<<t.transactionId<<" "<<t.message<<"\n";
    // }
    // static void log(string message){
    //     unique_lock<mutex>lck(Logger::m);
    //     cout<<message<<endl;
    // }
};

class WorkerThreadPool {
protected:
    int poolSize = 0;
    vector<thread> workers;
    mutex taskMutex;
    atomic<bool>shutdown{false};

public:
    condition_variable taskCV;
    WorkerThreadPool(int size = 0) : poolSize(size) {
        // CHANGED: no thread creation here (derived classes create worker threads).
    }

    virtual ~WorkerThreadPool() {
        // CHANGED: common shutdown + join logic so derived threads in `workers` are joined
        shutdown.store(true);
        taskCV.notify_all();
        for (auto &t : workers) {
            if (t.joinable()) t.join();
        }
    }

    // CHANGED: pure virtual submit removed; each subclass implements its submit/worker mechanism
    virtual void submit() = 0;
};

// -----------------------------
// Token bucket (thread-safe)
// -----------------------------
class TokenBucket {
    // this is the implementation of one the three types of token buckets. 
    double tokens;
    double capacity;
    double refillRate; 
    chrono::steady_clock::time_point lastRefill;
    mutex mtx;

public:
    TokenBucket(double rate = 5.0, double cap = 20.0)
        : tokens(cap), capacity(cap), refillRate(rate), lastRefill(chrono::steady_clock::now()) {
        // CHANGED: initialize tokens
    }

    bool allowRequest() {
        lock_guard<mutex> lock(mtx); // CHANGED: make thread-safe
        auto now = chrono::steady_clock::now();
        double seconds = chrono::duration<double>(now - lastRefill).count();

        // refill tokens
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
    // CHANGED: removed unused condition variable; we notify pool via pool->taskCV
    ProducerShard(WorkerThreadPool * pool) {
        this->pool = pool;
        rps = new TokenBucket();
    }

    ~ProducerShard() {
        delete rps; // CHANGED: free raw pointer (user requested no smart pointers)
    }

    // popTask now blocks until an entry exists
    InitialTransaction popTask() {
        unique_lock<mutex> lock(m);
        // CHANGED: wait until there's a task or the program is shutting down
        // Note: worker that calls this should coordinate with pool shutdown externally
        while (rec.empty()) {
            // simple spin-wait using condition variable could be better; we rely on taskCV from pool
            // but since we don't have access here, we'll busy-yield briefly to avoid tight spin.
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
            // CHANGED: return failure so caller knows it was rate-limited
        //     ErrorRecord tempError((string)obj.getTansactionId(),ErrorType::RateLimitExceeded,": Producer Shard",0);
        //    Logger::log(tempError);
            cout<<obj.getTansactionId()+"Producer Shard rate "<<'\n';
            return false;
        }
        if (rec.size() >= (size_t)MaxQueueSize) {
            // back pressure: reject; caller should act on this
            // ErrorRecord tempError(obj.getTansactionId(),ErrorType::BackpressureDrop,": Producer Shard",0);
            // Logger::log(tempError);
            cout<<obj.getTansactionId()+"Producer Shard  "<<"backPressure "<<"\n";
            return false;
        }
        rec.push_back(obj);
        lock.unlock();

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
// SUCCESS RECORD
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

// -----------------------------------------
// ERROR RECORD
// -----------------------------------------
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
        // Convert time_point to double (seconds since epoch)
        this->timestamp = chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count();
    }
    ErrorRecord() {}

    void SetErrorMessage(const string& layer) {
        message = "Refer to the Error code. Error occurred in " + layer;
    }
};

// -----------------------------------------
// RESPONSE STRUCT
// -----------------------------------------
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

ErrorType randomErrorType() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dist(1, 7); // Skip None

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

bool shouldFail(double failureRate = 0.3) {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<> dist(0.0, 1.0);
    return dist(gen) < failureRate;
}

// ======================================================================
// STRATEGY PATTERN BASE
// ======================================================================

class TransactionStrategy {
public:
    virtual Response makePayement(EncrpytedTransaction & temp) = 0; // CHANGED: return Response consistently
    virtual ~TransactionStrategy() {}
};

// ======================================================================
// STRATEGY IMPLEMENTATIONS
// ======================================================================

class CreditCardStrategy : public TransactionStrategy {
private:
    int latency = 10; // ms
public:
    CreditCardStrategy(){}

    Response makePayement(EncrpytedTransaction & temp) override {
        this_thread::sleep_for(chrono::milliseconds(latency));
        string txnId = temp.transactionId;
        if (shouldFail(0.3)) { // ~30% fail chance
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "Credit Card transaction failed", 0);
            rec.SetErrorMessage("CreditCardStrategy");
            return Response(rec, false, SuccessRec());
        }

        SuccessRec success(txnId, 0);
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

        if (shouldFail(0.2)) { // ~20% fail chance
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "UPI payment failed", 0);
            rec.SetErrorMessage("UPIStrategy");
            return Response(rec, false, SuccessRec());
        }

        SuccessRec success(txnId, 0);
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

        if (shouldFail(0.4)) { // ~40% fail chance
            ErrorType err = randomErrorType();
            ErrorRecord rec(txnId, err, "NetBanking payment failed", 0);
            rec.SetErrorMessage("NetBankingStrategy");
            return Response(rec, false, SuccessRec());
        }

        SuccessRec success(txnId, 0);
        return Response(ErrorRecord(), true, success);
    }

    void setLatency(int l) { this->latency = l; }
};

// ======================================================================
// PROCESSOR + HANDLER LAYER
// ======================================================================

class TransactionProcessor {
private:
    TransactionStrategy* processor = nullptr; // raw pointer owned by TransactionProcessor
public:
    TransactionProcessor() = default;

    // Replaces strategy and deletes old one (no smart ptrs)
    void setTransactionStrategy(TransactionStrategy* t) {
        if (processor) delete processor;
        processor = t;
    }

    Response makePayement(EncrpytedTransaction & temp) {
        if (processor != nullptr)
            return processor->makePayement(temp);

        // fallback response if no processor set
        ErrorRecord rec("UNKNOWN", ErrorType::Unknown, "No processor set", 0);
        rec.SetErrorMessage("TransactionProcessor");
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
    TransactionProcessor* processor; // pointer (lifetime managed externally)
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

        return processor->makePayement(temp);
    }
};



// -----------------------------
// ConsumerWorkerPool
// -----------------------------
class ConsumerWorkerPool : public WorkerThreadPool {
private:
    deque<EncrpytedTransaction> ActualTransactionQueue;
    deque<EncrpytedTransaction> BatchUploadDroppedTransactionQueue;
    TransactionHandlerAPI *gateway = nullptr; // CHANGED: pointer to API owned externally
    int MaxActualTQSize = 100;
    
    mutex m;
   
    atomic<bool> isWriting{false};
    condition_variable ConsumerCV;
public:
    CircuitBreaker * cb = nullptr;
    int BatchUploadDroppedTransactionQueueSizeLeft = 100;
    // CHANGED: pass pointer to gateway and initialize workers here
    ConsumerWorkerPool(int size, TransactionHandlerAPI *gateway, CircuitBreaker *breaker = nullptr)
        : WorkerThreadPool(size), gateway(gateway), cb(breaker) {
        if (poolSize <= 0) poolSize = size;
        for (int i = 0; i < poolSize; ++i) {
            workers.emplace_back([this] { this->workerPool(); });
        }
    }

    // Graceful destructor will join via WorkerThreadPool::~WorkerThreadPool()

    // worker thread loop
    void workerPool() {
        while (!shutdown.load()) {
            // check circuit breaker first (non-blocking check)
            if (cb && !cb->allowRequest()) {
                // circuit open: back off a bit
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }

            unique_lock<mutex> lck(m);
            // Wait until there is something to do or shutting down
            ConsumerCV.wait(lck,[this] {
                return !ActualTransactionQueue.empty() && !isWriting.load();
            });
            // ConsumerCV.wait_for(lck, chrono::milliseconds(100), [this] {
            //     return !ActualTransactionQueue.empty() && !isWriting.load();
            // });

            if (shutdown.load()) break;

            if (ActualTransactionQueue.empty()) {
                // spurious wake or nothing to do
                continue;
            }

            EncrpytedTransaction temp = ActualTransactionQueue.front();
            ActualTransactionQueue.pop_front();
            ConsumerCV.notify_all();
            lck.unlock();

            // process it (call gateway)
            Response resp = gateway->handlePayement(temp);
            if (resp.status) {
                if (cb) cb->recordSuccess();
                // here you will also have to maintain the logs also.
            } else {
                string temp = resp.rec1.transactionId+resp.rec1.message;
                // Logger::log(temp);
                cout<<temp<<'\n';
                // not all type of failure would be recorded as failure.
                if(resp.rec1.type == ErrorType::GatewayFailure || resp.rec1.type == ErrorType::GatewayTimeout){
                        if (cb) cb->recordFailure();

                        // like i can put in the retry queue again etc. I will fix it later on
                        // i will have to pput a retry logic here. // TODO: Add a retry logic here plz. 
                }
                else{
                    // means i have to drop this beacuse it is the issue fo permanent issue, unknown, none etc which is of know point of trying 
                    // just put it in the logs . 
                }
               
            }

            // TODO: logging, metrics, persistence of results
        }
    }

    // called by producer to bulk-upload
    void UploadFromTheUpstream(vector<EncrpytedTransaction> batch) {
        // CHANGED: mark writing state and then append while respecting max queue size
        isWriting.store(true);

        unique_lock<mutex> lck(m);
        // Wait until there is space or shutdown (bounded push)
        // why here wait_for ? what happens to the batch if that wait for period is over.
        ConsumerCV.wait_for(lck, chrono::milliseconds(500), [this] {
            return (int)ActualTransactionQueue.size() < MaxActualTQSize || shutdown.load();
        });

        // added this actually. 
        // while(BatchUploadDroppedTransactionQueue.size() && ActualTransactionQueue.size() < MaxActualTQSize){
        //       ActualTransactionQueue.push_back(BatchUploadDroppedTransactionQueue.front());
        //       BatchUploadDroppedTransactionQueue.pop_front();
        // }

        for (auto &itr : batch) {
            if ((int)ActualTransactionQueue.size() < MaxActualTQSize) {
                ActualTransactionQueue.push_back(itr);
            } else {
                // queue full: drop or log. For now we drop silently (but you should record metrics)
                // BatchUploadDroppedTransactionQueue.push_back(itr);
                // Logger::log(ErrorRecord(itr.transactionId,ErrorType::BatchUploadFailed,"happened batch upload",0));
                string temp = itr.transactionId+"happened batch upload";
                // Logger::log(temp);
                cout<<temp<<'\n';
            }
        }

        isWriting.store(false);
        lck.unlock();
        ConsumerCV.notify_all();
    }

    // Simple single item submit helper (not originally present but useful)
    bool submitOne(const EncrpytedTransaction &t) {
        unique_lock<mutex> lck(m);
        if ((int)ActualTransactionQueue.size() >= MaxActualTQSize) return false;
        ActualTransactionQueue.push_back(t);
        lck.unlock();
        ConsumerCV.notify_all();
        return true;
    }

    // Implement required abstract method (not used here)
    void submit() override {
        // no-op for this simplified example
    }
};

// -----------------------------
// ProducerWorkerPool
// -----------------------------
class ProducerWorkerPool : public WorkerThreadPool {
private:
    deque<EncrpytedTransaction> batchUploadQueue;
    thread batchUploader;
    int MaxSizeOfMidstreamQueue;
    condition_variable BatchUploadQueueFull;
    chrono::steady_clock::time_point lastUpload;
    ConsumerWorkerPool * CWorkerPool = nullptr;
    double BatchTime = 2.0;
    atomic<bool> Uploading{false};
    int BatchSize = 10; // CHANGED: initialize default batch size

public:
    // CHANGED: constructor receives a ProducerShard & to create worker threads
    ProducerWorkerPool(int size, ConsumerWorkerPool * CWorkerPool, ProducerShard & shard,int MaxSizeOfMidstreamQueue = 10 )
        : WorkerThreadPool(size), CWorkerPool(CWorkerPool) {
        if (poolSize <= 0) poolSize = size;
        this->MaxSizeOfMidstreamQueue = MaxSizeOfMidstreamQueue;
        // spawn worker threads that service the shard
        for (int i = 0; i < poolSize; ++i) {
            workers.emplace_back([this, &shard] { this->workerPool(shard); });
        }
        // start background uploader thread
        lastUpload = chrono::steady_clock::now();
        batchUploader = thread(&ProducerWorkerPool::BatchUploadCheck, this);
    }

    ~ProducerWorkerPool() {
        // join background uploader and allow base destructor to join worker threads
        shutdown.store(true);
        taskCV.notify_all();
        if (batchUploader.joinable()) batchUploader.join();
        // WorkerThreadPool destructor will join worker threads
    }

    // Simple conversion: encrypt initial transaction -> encrypted transaction
    EncrpytedTransaction encryptTransaction(InitialTransaction &it) {
        // CHANGED: simple mock encryption mapping fields; real encryption required
        EncrpytedTransaction et;
        et.modeOfPayment = it.modeOfPayment;
        et.amount = it.amount;
        et.transactionId = it.getTansactionId();
        return et;
    }

    void workerPool(ProducerShard & shard) {
        while (!shutdown.load()) {
            unique_lock<mutex> lock(taskMutex);
            // CHANGED: wait until shard has an item AND not uploading AND not shutdown
            
            taskCV.wait(lock,[&] {
                return (!shard.Isempty() && !Uploading.load()) || shutdown.load();
            });

            // taskCV.wait_for(lock, chrono::milliseconds(100), [&] {
            //     return (!shard.Isempty() && !Uploading.load()) || shutdown.load();
            // });

            // kind of delaying the rate.
            if(batchUploadQueue.size() >= MaxSizeOfMidstreamQueue){
                lock.unlock();
                this_thread::sleep_for(chrono::milliseconds(30));
                continue;
            }

            if (shutdown.load()) break;

            if (shard.Isempty()) {
                // nothing to do now
                continue;
            }

            // pop from shard (this will lock shard internally)
            InitialTransaction temp = shard.popTask();
            // convert to encrypted transaction
            EncrpytedTransaction et = encryptTransaction(temp);

            // push into batch queue under taskMutex
            batchUploadQueue.push_back(std::move(et));
            lock.unlock();
            // notify uploader thread maybe
            taskCV.notify_all();
        }
    }

    // background batch uploader
    void BatchUploadCheck() {
        while (!shutdown.load()) {
            this_thread::sleep_for(chrono::milliseconds(50)); // avoid tight loop

            auto elapsed = chrono::duration_cast<chrono::duration<double>>(chrono::steady_clock::now() - lastUpload).count();
            bool shouldUpload = (elapsed > BatchTime) || ((int)batchUploadQueue.size() >= BatchSize);
            if (!shouldUpload) continue;
            if(!CWorkerPool->cb->allowRequest()){
                // this is to prevent the any load on the downstream 
                // like trying to regulate the flow or reducing the flow to 
                // avoid flocking on the broken downstream service.

                continue;
            }

            // CHANGED: prepare batch under lock, but release before calling into consumer to avoid deadlocks
            vector<EncrpytedTransaction> batch;
            {
                unique_lock<mutex> lock(taskMutex);
                Uploading.store(true);
                int tempBatchSize = BatchSize;
                while (tempBatchSize > 0 && !batchUploadQueue.empty()) {
                    batch.push_back(std::move(batchUploadQueue.front()));
                    batchUploadQueue.pop_front();
                    --tempBatchSize;
                }
                Uploading.store(false);
            }

            // CHANGED: call consumer without holding Producer's taskMutex to avoid inter-pool deadlocks
            // this is also techincally coorect beacuse it will let the uploader task run in teh background. 
            
            // this is unncessarily trying to put on the things on the downstream 
            // even if the circuit is broken. 

            if (!batch.empty() && CWorkerPool) {
                CWorkerPool->UploadFromTheUpstream(batch);
            }
            lastUpload = chrono::steady_clock::now();
            // notify any waiting producer workers
            taskCV.notify_all();
        }
    }

    void submit() override {
        // no-op for this simplified example (producer threads actively pull from shard)
    }
};

// -----------------------------
// A minimal main to ensure compilation
// -----------------------------
int main() {
    // Setup basic components
    TransactionProcessor proc;
    TransactionHandlerAPI gateway(&proc);
    CircuitBreaker cb1;
    CircuitBreaker cb2;

    // Create consumer pool (2 workers)
    ConsumerWorkerPool consumerPool1(2, &gateway, &cb1);
    ConsumerWorkerPool consumerPool2(2, &gateway, &cb2);


    // Create a shard and producer pool (2 workers)
    ProducerShard shard1(&consumerPool1); // NOTE: shard expects WorkerThreadPool pointer; passing consumer for notify only
    ProducerShard shard2(&consumerPool2);
    ProducerWorkerPool producerPool1(2, &consumerPool1, shard1);
    ProducerWorkerPool producerPool2(2, &consumerPool2, shard2);

    // Submit some transactions into shard
    long long submitted = 0;
    long long failed = 0;
    while(true){
        
        InitialTransaction t1;
        string temp = "tx"+to_string(submitted);
        t1.setTranscationId(temp);
        t1.setModeOfPayment("UPI");
        t1.setAmount(10.0f);
        submitted++;
        bool ok;
        if(submitted%2){
            ok= shard1.Submit(t1);
        }
        else{
            ok= shard2.Submit(t1);
        }
       
        if (!ok) {failed ++; cout << "Submit t1 failed (rate limit/backpressure)\n";}
        this_thread::sleep_for(chrono::milliseconds(80));
        cout<<"total submitted "<<submitted<<" failed : "<<failed<<"\n";
    }
   
    cout<<"total submitted "<<submitted<<" failed : "<<failed<<"\n";

    // Let system run briefly
    // this_thread::sleep_for(chrono::seconds(2));

    // Shutdown happens in destructors
    return 0;
}
