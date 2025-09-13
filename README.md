# 🚀 Chow Transaction Processing System

A **high-performance, multi-threaded transaction processing system** built in C++ that demonstrates enterprise-level concurrent programming, design patterns, and real-time payment processing architecture.

## 📋 Table of Contents

- [🏗️ System Architecture](#️-system-architecture)
- [🎯 Key Features](#-key-features)
- [🛠️ Technologies Used](#️-technologies-used)
- [📂 Project Structure](#-project-structure)
- [🔧 Installation & Setup](#-installation--setup)
- [🚀 Quick Start](#-quick-start)
- [🏛️ Design Patterns](#️-design-patterns)
- [🧵 Threading Architecture](#-threading-architecture)
- [📊 System Components](#-system-components)
- [📈 Performance Features](#-performance-features)
- [🔒 Thread Safety](#-thread-safety)
- [📝 Usage Examples](#-usage-examples)
- [🐛 Debugging & Monitoring](#-debugging--monitoring)
- [🤝 Contributing](#-contributing)
- [📄 License](#-license)

## 🏗️ System Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│   CLIENT        │ -> │   PROCESSOR      │ -> │     HANDLER         │
│                 │    │                  │    │                     │
│ • Generate TX   │    │ • Logger->Queue  │    │ • Payment Strategy  │
│ • Push to Logger│    │ • Transfer Data  │    │ • Process Payments  │
│ • Every 2s      │    │ • Queue Mgmt     │    │ • Update Status     │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
         |                        |                         |
         v                        v                         v
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│   LOGGER        │    │ TRANSACTION      │    │   PAYMENT           │
│   SINGLETON     │    │ QUEUE            │    │   STRATEGIES        │
│                 │    │                  │    │                     │
│ • Thread-safe   │    │ • Thread-safe    │    │ • UPI Strategy      │
│ • Token Gen     │    │ • FIFO Queue     │    │ • Credit Card       │
│ • Decorators    │    │ • Condition Vars │    │ • Net Banking       │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

## 🎯 Key Features

### 🏢 **Enterprise-Grade Architecture**
- **Multi-threaded Processing**: Concurrent transaction handling
- **Thread-Safe Operations**: Mutex-protected shared resources
- **Producer-Consumer Pattern**: Efficient data flow management
- **Real-time Monitoring**: Live system statistics and performance metrics

### 💳 **Payment Processing**
- **Multiple Payment Methods**: UPI, Credit Card, Net Banking
- **Strategy Pattern Implementation**: Pluggable payment processors
- **Transaction State Management**: Complete lifecycle tracking
- **Automatic Token Generation**: Unique transaction IDs with timestamps

### 🔒 **Concurrency & Safety**
- **Singleton Pattern**: Centralized logger and queue management
- **Condition Variables**: Efficient thread synchronization
- **Deadlock Prevention**: Proper lock ordering and timeouts
- **Race Condition Mitigation**: Atomic operations and mutex protection

### 📊 **Monitoring & Debugging**
- **Thread-Safe Console Output**: Clean, organized logging
- **Real-time Statistics**: System performance metrics
- **Transaction Tracing**: End-to-end transaction visibility
- **Professional Logging**: Timestamped, categorized output

## 🛠️ Technologies Used

- **Language**: C++17/20
- **Threading**: `std::thread`, `std::mutex`, `std::condition_variable`
- **Containers**: `std::vector`, `std::deque`, `std::string`
- **Concurrency**: Thread-safe singleton patterns
- **Build System**: g++ with manual compilation
- **Design Patterns**: Singleton, Strategy, Decorator, Producer-Consumer

## 📂 Project Structure

```
ChowTransactionService/
├── 📁 client/
│   ├── AggressiveTestUser.cpp      # Transaction generator client
│   └── simpleTestuser.cpp          # Simple test client (placeholder)
├── 📁 server/
│   ├── 📁 include/
│   │   ├── Logger.h                # Core logging system headers
│   │   └── TransactionQueue.h      # Thread-safe queue implementation
│   ├── 📁 headerImplementation/
│   │   └── clientApiEndPoint.cpp   # Core system implementations
│   └── 📁 processor/
│       ├── initalProcessor.cpp     # Logger->Queue transfer logic
│       ├── transactionProcessing.cpp # Payment processing logic
│       └── overview.txt            # System overview documentation
├── 📁 logs/
│   ├── inputTranscations.txt       # Input transaction logs
│   ├── internalDatabaseRecord.txt  # Internal system records
│   └── overview.txt                # Logging system overview
├── 📁 .vscode/
│   └── settings.json               # IDE configuration
├── unified_main.cpp                # Combined system (all components)
└── README.md                       # This file
```

## 🔧 Installation & Setup

### Prerequisites
- **Compiler**: g++ (GCC 7+ or Clang 6+) with C++17 support
- **OS**: Linux, macOS, or Windows (with MinGW)
- **RAM**: Minimum 512MB available memory
- **CPU**: Multi-core processor recommended

### Build Instructions

1. **Clone the repository**
```bash
git clone <repository-url>
cd ChowTransactionService
```

2. **Compile individual components**
```bash
# Client only
g++ client/AggressiveTestUser.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o client_test

# Processor only  
g++ server/processor/initalProcessor.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o processor_test

# Handler only
g++ server/processor/transactionProcessing.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o handler_test
```

3. **Compile unified system (Recommended)**
```bash
g++ unified_main.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o unified_system
```

## 🚀 Quick Start

### Running the Complete System
```bash
# Compile and run the unified system
g++ unified_main.cpp server/headerImplementation/clientApiEndPoint.cpp -I server/include -o unified_system
./unified_system
```

### Expected Output
```
████████████████████████████████████████████████████████████████████████████████
             🚀 CHOW TRANSACTION PROCESSING SYSTEM 🚀
████████████████████████████████████████████████████████████████████████████████

[14:32:15.123] [CLIENT        ] 📝 Transaction generated | Count: #1 | Amount: $234.3
[14:32:15.125] [PROCESSOR     ] 📦 Transaction moved successfully | Logger: 0 | Queue: 1
[14:32:15.127] [HANDLER       ] ⚡ Processing transaction | ID: 17349872... | Method: UPI
[14:32:15.129] [HANDLER       ] 💳 Payment completed | Status: ✅ SUCCESS
```

## 🏛️ Design Patterns

### 1. **Singleton Pattern**
```cpp
// Logger singleton ensures single instance across threads
Logger& Logger::getInstance() {
    static Logger instance; 
    return instance;
}
```

### 2. **Strategy Pattern**
```cpp
// Pluggable payment processing strategies
class TransactionStrategy {
public:
    virtual bool makePayement(float amount) = 0;
};

class UPIStrategy : public TransactionStrategy { /* ... */ };
class CreditCardStrategy : public TransactionStrategy { /* ... */ };
```

### 3. **Decorator Pattern**
```cpp
// Transaction decoration with backend keys
class DecoratorForTranSaction {
public:
    static ClientTranscationInstance decorate(tempTransaction& t);
};
```

### 4. **Producer-Consumer Pattern**
- **Producer**: `AggressiveTestUser` generates transactions
- **Buffer**: Thread-safe `Logger` and `TransactionQueue`
- **Consumer**: `TransactionHandler` processes payments

## 🧵 Threading Architecture

### Thread Roles

| Thread | Component | Responsibility | Frequency |
|--------|-----------|----------------|-----------|
| 🔵 **Client** | `AggressiveTestUser` | Generate transactions | Every 2 seconds |
| 🟢 **Processor** | `InitialProcessor` | Move Logger→Queue | Every 100ms |
| 🟠 **Handler** | `TransactionHandler` | Process payments | On-demand |
| 🟣 **Monitor** | `MonitoringThread` | System statistics | Every 10 seconds |

### Synchronization Mechanisms
- **Mutexes**: Protect shared data structures
- **Condition Variables**: Efficient thread waiting/notification
- **Atomic Operations**: Lock-free counters and flags
- **RAII**: Automatic resource management

## 📊 System Components

### Core Classes

#### `Logger` (Singleton)
- **Purpose**: Central transaction logging and token generation
- **Thread Safety**: Mutex-protected operations
- **Features**: Unique ID generation, decorator pattern integration

#### `TransactionQueue` (Singleton)  
- **Purpose**: Thread-safe FIFO queue for transaction processing
- **Implementation**: `std::deque` with mutex protection
- **Operations**: Push, pop, size checking with condition variables

#### `TransactionProcessor`
- **Purpose**: Strategy pattern coordinator for payments
- **Features**: Dynamic strategy selection, pluggable processors

#### Payment Strategies
- **`UPIStrategy`**: Mobile payment processing
- **`CreditCardStrategy`**: Credit card transaction handling  
- **`NetBankingStrategy`**: Online banking integration

## 📈 Performance Features

### Optimization Techniques
- **Lock Granularity**: Fine-grained locking to minimize contention
- **Condition Variables**: Avoid busy-waiting and CPU waste
- **Memory Pool**: Efficient object allocation (planned)
- **Batch Processing**: Group operations for better throughput

### Performance Metrics
- **Transaction Rate**: ~30 transactions per minute (configurable)
- **Latency**: Sub-millisecond processing per transaction
- **Throughput**: Scales with CPU cores
- **Memory Usage**: ~10MB baseline + transaction data

## 🔒 Thread Safety

### Safety Mechanisms
1. **Mutex Protection**: All shared data structures
2. **RAII Locks**: `std::lock_guard` for exception safety
3. **Condition Variables**: Prevent race conditions in waiting
4. **Atomic Counters**: Lock-free increment operations

### Critical Sections
```cpp
// Thread-safe transaction logging
void Logger::push(tempTransaction t) {
    unique_lock<mutex> ul(mtx);
    // ... critical section ...
    Logger::isEmpty.notify_one();
}
```

## 📝 Usage Examples

### Custom Transaction Generation
```cpp
// Create custom transaction
tempTransaction customTx;
customTx.setAmount(500.0);
customTx.setModeOfPayement("CreditCard");
customTx.setName("CustomUser");
customTx.setPhoneNumber("1234567890");

// Push to system
Logger::getInstance().push(customTx);
```

### Adding New Payment Strategy
```cpp
class PayPalStrategy : public TransactionStrategy {
public:
    bool makePayement(float amount) override {
        // PayPal processing logic
        return processPayPalPayment(amount);
    }
};
```

## 🐛 Debugging & Monitoring

### Built-in Monitoring
- **Real-time Statistics**: Queue sizes, transaction counts
- **Thread Status**: Active/idle state monitoring  
- **Performance Metrics**: Processing rates and latencies
- **Error Tracking**: Exception handling and reporting

### Debug Output Control
```cpp
// Thread-safe logging with categories
printThreadSafe("DEBUG", "Transaction processed", 
               "ID: " + txId + " | Status: SUCCESS");
```

### Log Files
- `logs/inputTranscations.txt`: Input transaction records
- `logs/internalDatabaseRecord.txt`: Internal system state
- `logs/overview.txt`: System operational logs

## 🔮 Future Enhancements

### Planned Features
- **Database Integration**: PostgreSQL/MySQL support
- **REST API**: HTTP endpoints for external integration
- **Configuration Management**: XML/JSON config files
- **Load Balancing**: Multi-instance deployment
- **Metrics Dashboard**: Real-time web UI
- **Message Queues**: RabbitMQ/Apache Kafka integration

### Performance Improvements
- **Memory Pool**: Custom allocators for transaction objects
- **Lock-free Algorithms**: CAS-based data structures
- **NUMA Optimization**: CPU affinity and memory locality
- **Batch Processing**: Grouped transaction handling

## 🤝 Contributing

### Development Workflow
1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Follow coding standards (see `.vscode/settings.json`)
4. Add comprehensive tests
5. Commit changes (`git commit -m 'Add amazing feature'`)
6. Push to branch (`git push origin feature/amazing-feature`)
7. Open Pull Request

### Coding Standards
- **C++ Style**: Modern C++17/20 features
- **Naming**: camelCase for variables, PascalCase for classes
- **Comments**: Comprehensive documentation for public APIs
- **Threading**: Always use RAII for lock management

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 📞 Contact & Support

- **Author**: Aditya (Original Architecture & Implementation)
- **Documentation**: Enhanced with AI assistance
- **Issues**: Please report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions

---

### 🌟 **Star this repository if you found it helpful!**

*Built with ❤️ using modern C++ and enterprise design patterns*
