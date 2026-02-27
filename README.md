# MT5 Deal Processor

A multi-threaded central Deal Processor that demonstrates integration with the MetaTrader 5 Manager API. Built in C++17 as a self-contained demo for the Hentec Trading C++ Developer position.

## Build & Run

### Prerequisites

- C++17 compiler (GCC 8+, Clang 10+, Apple Clang 12+)
- CMake 3.16+

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Run

```bash
# Normal simulation: 5 clients, 10 requests each
./deal_processor

# High-frequency burst test: 10 clients, 20 requests each, minimal delay
./deal_processor --burst
```

Log output is written to both the console and `deal_processor.log`.

---

## Expected Output

### Startup

```
================================================================
  MT5 Deal Processor - Self-Contained Demo
  Hentec Trading - C++ Developer Task
================================================================
  NOTE: ~10% of requests are INTENTIONALLY invalid to
  demonstrate error handling (tagged [INTENTIONAL-BAD-REQUEST]).
  ~3% of MT API calls simulate connection failures for retry
  testing. All other errors are real validation failures.
================================================================

[2026-02-27 12:46:53.255] [INFO ] [Thread-0x200f42080] Connecting to MT5 server...
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080] Connected to MT5 server successfully
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080] Available symbols: 6
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080]   EURUSD Bid=1.08447 Ask=1.08462 Volume=[0.01-100.00]
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080]   GBPUSD Bid=1.26272 Ask=1.26292 Volume=[0.01-100.00]
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080]   XAUUSD Bid=2035.50 Ask=2036.00 Volume=[0.01-50.00]
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080]   ...
[2026-02-27 12:46:53.347] [INFO ] [Thread-0x200f42080] Account #12345 Balance=$100000 FreeMargin=$100000
```

### Trade Processing (multiple workers handling concurrent requests)

```
[INFO ] [Thread-0x16dfe3000] Request received: [Client-2-000000] Client-2 BUY EURUSD 0.12 lots SL=0.995 TP=1.005
[INFO ] [Thread-0x16e0fb000] Request received: [Client-4-000004] Client-4 BUY USDCAD 0.34 lots
[INFO ] [Thread-0x16ddb3000] Worker-1 EXECUTED: [Client-4-000004] SUCCESS Ticket=#100000 Price=1.35736
[INFO ] [Thread-0x16de3f000] Worker-2 EXECUTED: [Client-1-000001] SUCCESS Ticket=#100001 Price=1.26319
[INFO ] [Thread-0x16dd27000] Worker-0 EXECUTED: [Client-2-000000] SUCCESS Ticket=#100002 Price=1.08467
```

### Error Handling Examples

```
-- Intentional bad request (invalid volume, tagged in log):
[INFO ] Request received: [INTENTIONAL-BAD-REQUEST] [Client-4-000005] Client-4 SELL EURUSD 0 lots
[WARN ] Worker-1 validation failed: [Client-4-000005] INVALID_PARAMS Error: Invalid volume: 0.000000

-- Intentional bad request (unknown symbol):
[INFO ] Request received: [INTENTIONAL-BAD-REQUEST] [Client-1-000008] Client-1 BUY INVALID 0.1 lots
[WARN ] Worker-0 validation failed: [Client-1-000008] INVALID_PARAMS Error: Unknown symbol: INVALID

-- Simulated connection failure with retry:
[WARN ] Worker-3 retrying Client-3-000003 (attempt 2/4, delay=100ms)
[INFO ] Worker-3 EXECUTED: [Client-3-000003] SUCCESS Ticket=#100005 Price=1.26320
```

### Execution Summary

```
  Timing:
    Client submission phase: 1450ms
    Total processing time:   2030ms
    Requests processed:      50
    Throughput:              24.6 req/sec

================================================================
                    EXECUTION SUMMARY
================================================================
  Total Requests:   50
  Successful:       46
  Rejected:         0
  Errors:           4
  Duplicates:       0
  Success Rate:     92.0%
================================================================

  Per-Client Breakdown:
  Client      Total   OK      Fail    Dup
  --------------------------------------------
  Client-1    10      9       1       0
  Client-2    10      10      0       0
  Client-3    10      9       1       0
  Client-4    10      8       2       0
  Client-5    10      10      0       0

  Request ID -> MT Ticket Mapping (successful trades):
  Request ID            Ticket      Price
  --------------------------------------------------
  Client-1-000000       #100003     149.86502
  Client-2-000001       #100000     1.35740
  Client-3-000003       #100002     1.08466
  Client-4-000005       #100005     1.35722
  ...
================================================================
```

> **Note**: ~10% of requests are intentionally invalid (tagged `[INTENTIONAL-BAD-REQUEST]` in logs)
> to demonstrate error handling. ~3% of MT API calls simulate connection timeouts to test
> the retry mechanism. Actual success rate for valid requests is ~97%.

---

## Architecture

```
Client-1  Client-2  ...  Client-N        (std::thread per client)
   |         |              |
   v         v              v
┌──────────────────────────────────┐
│       ThreadSafeQueue            │     std::queue + mutex + condition_variable
│  (thread-safe request buffer)    │
└──────────────┬───────────────────┘
               v
┌──────────────────────────────────┐
│         DealProcessor            │     Coordinator
│  ┌────────┐ ┌────────┐ ┌──────┐ │
│  │Worker-0│ │Worker-1│ │Wkr-N │ │     std::thread pool (configurable)
│  └───┬────┘ └───┬────┘ └──┬───┘ │
└──────┼──────────┼─────────┼──────┘
       v          v         v
┌──────────────────────────────────┐
│          Validator               │     Pre-execution checks
│  (symbol, volume, margin, dedup) │
└──────────────┬───────────────────┘
               v
┌──────────────────────────────────┐
│       IMTBrokerAPI (interface)   │     Abstract MT5 Manager API
│         MockMTAPI (impl)         │     Simulated broker behavior
└──────────────┬───────────────────┘
               v
┌──────────────┴───────────────────┐
│  ResultTracker  │     Logger     │     Thread-safe tracking & logging
│  (req→ticket)   │  (file+stdout) │
└─────────────────┴────────────────┘
```

---

## MT5 Manager API Methods Used

The `IMTBrokerAPI` interface maps directly to the MT5 Manager API SDK:

| Interface Method | MT5 Manager API | Purpose |
|---|---|---|
| `connect()` | `IMTManagerAPI::Connect()` | Establish connection to the MT5 server |
| `disconnect()` | `IMTManagerAPI::Disconnect()` | Graceful disconnection |
| `getSymbolInfo()` | `IMTManagerAPI::SymbolGet()` + `SymbolInfoGet()` | Validate symbol exists, get trading specs and live prices |
| `getAccountInfo()` | `IMTManagerAPI::UserAccountGet()` | Check account balance and free margin |
| `executeTrade()` | **`IMTManagerAPI::DealerSend()`** | Execute trade via dealer request |
| `getTicketInfo()` | `IMTManagerAPI::DealGet()` | Verify deal execution, retrieve ticket details |
| `getSymbols()` | `IMTManagerAPI::SymbolNext()` | Enumerate available trading symbols |

### Why DealerSend()?

`DealerSend()` is the correct method for manager/dealer-initiated trades because:

1. It passes through **all server-side validations** (margin check, symbol trade limits, session filters, price validation)
2. It respects **trading hours** and **symbol restrictions**
3. It performs **price verification** against current market
4. It returns a proper **deal ticket** on success

This is different from direct deal creation methods which bypass server-side checks.

### Execution Flow (per request)

```
1. SymbolGet(symbol)           → Validate symbol exists and is tradeable
2. UserAccountGet(login)       → Check sufficient free margin
3. SymbolInfoGet(symbol)       → Get current bid/ask for price validation
4. DealerSend(trade_request)   → Execute trade through all validations
5. DealGet(ticket)             → Confirm execution, get fill details
```

---

## Threading Model

### Producer-Consumer Pattern

The system uses a classic **producer-consumer** pattern with a bounded thread pool:

- **Producers**: N client threads push `TradeRequest` objects into a shared queue
- **Buffer**: `ThreadSafeQueue` (std::queue + mutex + condition_variable)
- **Consumers**: M worker threads pop requests and process them independently

### Synchronization

| Component | Mechanism | Purpose |
|---|---|---|
| `ThreadSafeQueue` | `std::mutex` + `std::condition_variable` | Blocking pop, thread-safe push |
| `Logger` | `std::mutex` | Serialized console/file output |
| `ResultTracker` | `std::mutex` | Thread-safe result storage |
| `Validator` | `std::mutex` | Duplicate request detection set |
| `MockMTAPI` | `std::mutex` (account + RNG) | Simulated margin tracking |

### Shutdown Sequence

1. Client threads finish submitting → join
2. `DealProcessor::stop()` sets `running_ = false`
3. `ThreadSafeQueue::shutdown()` wakes all blocked workers
4. Workers drain remaining items, then exit
5. All worker threads joined → clean shutdown

No requests are lost during shutdown because the queue is drained before workers exit.

---

## Request Processing Flow

```
Client submits request
        │
        v
   ┌─ Enqueue in ThreadSafeQueue ─┐
   │                               │
   v                               │
Worker dequeues request            │  (blocks via condition_variable)
   │                               │
   v                               │
Validator checks:                  │
  ├─ Duplicate request? ──────> DUPLICATE error
  ├─ Valid parameters? ────────> INVALID_PARAMS error
  ├─ Symbol exists? ──────────> INVALID_PARAMS error
  ├─ Volume in range? ────────> INVALID_PARAMS error
  └─ SL/TP valid? ────────────> INVALID_PARAMS error
   │
   v (all checks pass)
MT API executeTrade() (DealerSend)
   │
   ├─ Success ──────────────> Record ticket, log, callback
   │
   ├─ Transient failure ───> Retry with exponential backoff
   │   └─ Retry 1: 100ms delay
   │   └─ Retry 2: 200ms delay
   │   └─ Retry 3: 400ms delay
   │   └─ All failed ──────> RETRY_EXHAUSTED error
   │
   └─ Permanent failure ──> Record error, log, callback
```

---

## Error Handling

| Error Type | Detection | Response |
|---|---|---|
| Duplicate request | Request ID seen before (set lookup) | Immediate rejection, no MT API call |
| Invalid symbol | `SymbolGet()` returns not found | Validation error before execution |
| Invalid volume | Outside min/max or wrong step | Validation error before execution |
| Insufficient margin | `UserAccountGet()` free margin too low | Margin error from MT API |
| Connection timeout | Simulated random failure | Retry with exponential backoff |
| Trade rejection | Server-side rejection via `DealerSend()` | Retry (may be transient) |
| Empty parameters | Null/empty client ID, symbol | Validation error before execution |

---

## Bonus Features Implemented

1. **Request ID → MT Ticket ID mapping**: `ResultTracker` maintains a full map of every client request to its corresponding MT deal ticket. Printed in the summary.

2. **Retry mechanism**: Failed trades with transient errors (connection timeouts, temporary rejections) are retried up to 3 times with **exponential backoff** (100ms → 200ms → 400ms).

3. **High-frequency burst test**: Run with `--burst` flag to simulate 200 requests across 10 clients with near-zero delay. Verifies no lost requests and measures throughput.

---

## Project Structure

```
src/
├── main.cpp                    Entry point + simulation harness
├── models/
│   ├── TradeRequest.h          Trade request data structure
│   └── TradeResult.h           Trade result data structure
├── queue/
│   └── ThreadSafeQueue.h       Lock-based concurrent queue (header-only)
├── processor/
│   ├── DealProcessor.h/cpp     Central processor + worker pool
│   └── Validator.h             Pre-execution validation layer
├── mt_api/
│   ├── IMTBrokerAPI.h          Abstract MT5 Manager API interface
│   └── MockMTAPI.h/cpp         Simulated broker (realistic behavior)
├── logger/
│   └── Logger.h/cpp            Thread-safe dual-output logger
├── tracker/
│   └── ResultTracker.h/cpp     Result storage + statistics
└── client/
    └── ClientSimulator.h/cpp   Multi-threaded client simulation
```

---

## Adapting to Production MT5

To connect to a real MT5 server, implement `IMTBrokerAPI` using the MetaQuotes Manager API SDK:

```cpp
class RealMT5API : public IMTBrokerAPI {
    IMTManagerAPI* manager_;  // MetaQuotes SDK handle
public:
    bool connect(...) override {
        // manager_->Connect(server, login, password);
    }
    TradeResult executeTrade(const TradeRequest& req) override {
        // IMTDealerRequest* dealer = ...;
        // dealer->Action(IMTDealerRequest::ACTION_DEAL);
        // dealer->Symbol(req.symbol.c_str());
        // dealer->Volume(req.volume);
        // manager_->DealerSend(dealer, answer);
    }
};
```

The rest of the system (queue, processor, validator, tracker, logger) remains unchanged.
