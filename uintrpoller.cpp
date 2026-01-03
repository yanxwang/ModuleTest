#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <syscall.h>
#include <x86gprintrin.h>

// UINTR syscall numbers
#define __NR_uintr_register_handler	471
#define __NR_uintr_unregister_handler	472
#define __NR_uintr_create_fd		473
#define __NR_uintr_register_sender	474
#define __NR_uintr_unregister_sender	475
#define __NR_uintr_wait			476

// UINTR syscall wrappers
static inline long uintr_register_handler(void* handler, unsigned int flags) {
    return syscall(__NR_uintr_register_handler, handler, flags);
}

static inline long uintr_unregister_handler(unsigned int flags) {
    return syscall(__NR_uintr_unregister_handler, flags);
}

static inline long uintr_create_fd(unsigned long long vector, unsigned int flags) {
    return syscall(__NR_uintr_create_fd, vector, flags);
}

static inline long uintr_register_sender(int fd, unsigned int flags) {
    return syscall(__NR_uintr_register_sender, fd, flags);
}

static inline long uintr_unregister_sender(int ipi_idx, unsigned int flags) {
    return syscall(__NR_uintr_unregister_sender, ipi_idx, flags);
}

static inline long uintr_wait(unsigned int flags) {
    return syscall(__NR_uintr_wait, flags);
}

std::mutex print_mutex;

// -------------------------------------------------------------
// Forward declaration
// -------------------------------------------------------------
template<typename T> struct LockFreeQueue;

// -------------------------------------------------------------
// Request / Response messages
// -------------------------------------------------------------
struct VersionRequest {
    uint64_t key;           // key to determine worker
    uint64_t timestamp;     // for latency calculation
    uint64_t sequence_number; // global ordering assigned by synchronizer
    uint32_t client_id;     // which client sent this
    LockFreeQueue<struct VersionResponse>* resp_q_ptr; // pointer to client's response queue
};

struct VersionResponse {
    uint64_t key;           // copy of request key
    uint64_t timestamp;     // original request timestamp
    uint64_t sequence_number; // copy of sequence number
};

// -------------------------------------------------------------
// High-precision timing
// -------------------------------------------------------------
static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static double estimate_cpu_freq_ghz() {
    auto start_time = std::chrono::steady_clock::now();
    uint64_t start_tsc = rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto end_time = std::chrono::steady_clock::now();
    uint64_t end_tsc = rdtsc();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    uint64_t cycles = end_tsc - start_tsc;
    return (double)cycles / (double)duration_ns;  // GHz
}

// -------------------------------------------------------------
// Lock-free SPSC queue
// -------------------------------------------------------------
template<typename T>
struct LockFreeQueue {
    T* entries;
    size_t size;
    std::atomic<uint64_t> head;
    std::atomic<uint64_t> tail;

    LockFreeQueue(size_t queue_size) : size(queue_size) {
        entries = new T[queue_size];
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
    }
    ~LockFreeQueue() { delete[] entries; }

    bool enqueue(const T& item) {
        uint64_t current_head = head.load(std::memory_order_relaxed);
        uint64_t next_head = (current_head + 1) % size;
        if (next_head == tail.load(std::memory_order_acquire))
            return false;
        entries[current_head] = item;
        head.store(next_head, std::memory_order_release);
        return true;
    }

    bool dequeue(T& item) {
        uint64_t current_tail = tail.load(std::memory_order_relaxed);
        if (current_tail == head.load(std::memory_order_acquire))
            return false;
        item = entries[current_tail];
        tail.store((current_tail + 1) % size, std::memory_order_release);
        return true;
    }

    // Check if queue is empty (for poller)
    bool is_empty() const {
        return tail.load(std::memory_order_acquire) == head.load(std::memory_order_acquire);
    }
};

// -------------------------------------------------------------
// Latency tracking
// -------------------------------------------------------------
struct LatencyTracker {
    uint64_t* samples;
    std::atomic<size_t> count{0};
    size_t max_samples;
    size_t sample_interval;

    LatencyTracker(size_t max, size_t interval)
        : max_samples(max), sample_interval(interval) {
        samples = new uint64_t[max_samples];
    }
    ~LatencyTracker() { delete[] samples; }

    void record(uint64_t latency_cycles) {
        size_t idx = count.fetch_add(1, std::memory_order_relaxed);
        if (idx < max_samples) samples[idx] = latency_cycles;
    }

    void print_statistics(double cpu_freq_ghz, uint32_t client_id) {
        size_t n = std::min(count.load(), max_samples);
        if (n == 0) return;
        std::sort(samples, samples + n);
        uint64_t sum = 0;
        for (size_t i = 0; i < n; i++) sum += samples[i];
        double avg_cycles = (double)sum / n;
        uint64_t p50 = samples[n / 2];
        uint64_t p90 = samples[(size_t)(n * 0.90)];
        uint64_t p99 = samples[(size_t)(n * 0.99)];
        auto to_ns = [cpu_freq_ghz](uint64_t cycles) { return cycles / cpu_freq_ghz; };

        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "[Client " << client_id << "] Latency samples: " << n << "\n";
        std::cout << " Avg: " << to_ns((uint64_t)avg_cycles) << " ns\n";
        std::cout << " p50: " << to_ns(p50) << " ns\n";
        std::cout << " p90: " << to_ns(p90) << " ns\n";
        std::cout << " p99: " << to_ns(p99) << " ns\n";
    }
};

// -------------------------------------------------------------
// Client channel
// -------------------------------------------------------------
struct ClientChannel {
    LockFreeQueue<VersionRequest>* req_q;
    LockFreeQueue<VersionResponse>* resp_q;
    LatencyTracker* latency;
    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};

    // UINTR file descriptor for this client
    int uintr_fd;

    ClientChannel(size_t queue_size, size_t sample_interval) {
        req_q = new LockFreeQueue<VersionRequest>(queue_size);
        resp_q = new LockFreeQueue<VersionResponse>(queue_size);
        latency = new LatencyTracker(10000000, sample_interval);
        uintr_fd = -1;
    }
    ~ClientChannel() {
        delete req_q;
        delete resp_q;
        delete latency;
    }
};

// -------------------------------------------------------------
// CPU pinning helper (same as baseline)
// -------------------------------------------------------------
void pin_thread_to_cpu(std::thread& th, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int rc = pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cerr << "Error pinning thread to CPU " << cpu_id << "\n";
    }
}

// -------------------------------------------------------------
// Worker with Direct Ring Buffer Handoff
// -------------------------------------------------------------
struct Worker {
    static constexpr size_t BUFFER_SIZE = 16;  // Small buffer for direct handoff

    alignas(64) VersionRequest buffer[BUFFER_SIZE];
    alignas(64) std::atomic<uint64_t> write_idx{0};
    alignas(64) std::atomic<uint64_t> read_idx{0};

    // Synchronizer tries to handoff (non-blocking)
    bool try_handoff(const VersionRequest& req) {
        uint64_t w = write_idx.load(std::memory_order_relaxed);
        uint64_t r = read_idx.load(std::memory_order_acquire);

        // Check if full
        if ((w + 1) % BUFFER_SIZE == r % BUFFER_SIZE) {
            return false;  // Full, skip without waiting
        }

        buffer[w % BUFFER_SIZE] = req;  // Direct copy
        write_idx.store(w + 1, std::memory_order_release);
        return true;
    }

    // Worker tries to read (non-blocking)
    bool try_get_request(VersionRequest& req) {
        uint64_t r = read_idx.load(std::memory_order_relaxed);
        uint64_t w = write_idx.load(std::memory_order_acquire);

        if (r == w) return false;  // Empty

        req = buffer[r % BUFFER_SIZE];
        read_idx.store(r + 1, std::memory_order_release);
        return true;
    }
};

void worker_thread_func(Worker* w, std::atomic<bool>& stop_flag) {
    while (!stop_flag.load(std::memory_order_relaxed)) {
        VersionRequest req;
        if (w->try_get_request(req)) {
            // Create response
            VersionResponse resp;
            resp.key = req.key;
            resp.timestamp = req.timestamp;
            resp.sequence_number = req.sequence_number;

            // Write response back to client using the pointer in the request
            while (!req.resp_q_ptr->enqueue(resp)) {
                if (stop_flag.load(std::memory_order_relaxed)) break;
            }
        }
    }
}

// -------------------------------------------------------------
// Synchronizer (UNCHANGED from baseline)
// -------------------------------------------------------------
void synchronizer_thread_func(std::vector<ClientChannel*>& clients,
                              std::vector<Worker*>& workers,
                              std::atomic<bool>& stop_flag,
                              uint64_t& total_forwarded) {
    size_t num_clients = clients.size();
    size_t num_workers = workers.size();
    uint64_t global_seq = 0;  // Global sequence number
    uint64_t forwarded = 0;
    uint64_t last_forwarded = 0;
    uint64_t last_consumed = 0;

    for (size_t sec = 1;; ++sec) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count() < 1) {
            // Round-robin polling of all client request queues
            for (size_t i = 0; i < num_clients; ++i) {
                VersionRequest req;
                if (clients[i]->req_q->dequeue(req)) {
                    // Assign global sequence number
                    req.sequence_number = global_seq++;
                    req.client_id = i;
                    req.resp_q_ptr = clients[i]->resp_q;

                    // Select worker based on key (modulo is fine for uniform distribution)
                    size_t wid = req.key % num_workers;

                    // Direct handoff (non-blocking)
                    if (workers[wid]->try_handoff(req)) {
                        ++forwarded;
                    }
                    // If handoff fails (buffer full), request is dropped
                }
            }
            if (stop_flag.load()) break;
        }

        // Per-second throughput reporting
        uint64_t total_consumed = 0;
        for (auto& ch : clients) total_consumed += ch->consumed.load();

        uint64_t sync_delta = forwarded - last_forwarded;
        uint64_t system_delta = total_consumed - last_consumed;

        last_forwarded = forwarded;
        last_consumed = total_consumed;

        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "Time " << sec << "s: " << sync_delta << " req/sec\n";
        std::cout << "  System: " << system_delta << " req/sec\n";

        if (stop_flag.load()) break;
    }
    total_forwarded = forwarded;
}

// -------------------------------------------------------------
// Client - Request Thread (UNCHANGED from baseline)
// -------------------------------------------------------------
void client_request_thread_func(ClientChannel* ch, uint32_t client_id, std::atomic<bool>& stop_flag) {
    // Random number generator for uniform random keys
    std::random_device rd;
    std::mt19937_64 gen(rd() + client_id);  // Seed with client_id for different sequences
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    uint64_t local_produced = 0;
    size_t sample_interval = ch->latency->sample_interval;

    while (!stop_flag.load(std::memory_order_relaxed)) {
        VersionRequest req;
        req.key = dist(gen);  // Random uniform key

        // Sample latency periodically
        if ((local_produced % sample_interval) == 0)
            req.timestamp = rdtsc();
        else
            req.timestamp = 0;

        if (ch->req_q->enqueue(req)) {
            ++local_produced;
            ch->produced.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// -------------------------------------------------------------
// Client - Response Thread (NEW: UINTR-based, no busy-poll)
// -------------------------------------------------------------
// UINTR handler (must be interrupt-safe)
volatile unsigned long uintr_wakeup_flag = 0;

void __attribute__ ((interrupt))
     __attribute__((target("general-regs-only", "inline-all-stringops")))
     uintr_response_handler(struct __uintr_frame *ui_frame, unsigned long long vector) {
    // Simply set flag to wake up the response thread
    uintr_wakeup_flag = 1;
}

void client_response_thread_func(ClientChannel* ch, uint32_t client_id,
                                  std::atomic<bool>& stop_flag, double cpu_freq_ghz) {
    // Register UINTR handler for this thread
    if (uintr_register_handler((void*)uintr_response_handler, 0) < 0) {
        std::cerr << "[Client " << client_id << "] Failed to register UINTR handler\n";
        return;
    }

    // Create UINTR file descriptor
    ch->uintr_fd = uintr_create_fd(0, 0);
    if (ch->uintr_fd < 0) {
        std::cerr << "[Client " << client_id << "] Failed to create UINTR fd\n";
        return;
    }

    // Enable interrupts
    _stui();

    while (!stop_flag.load(std::memory_order_relaxed)) {
        // Wait for UINTR signal from poller
        uintr_wait(0);

        // Drain all responses from the queue
        VersionResponse resp;
        while (ch->resp_q->dequeue(resp)) {
            // Measure latency if timestamp was recorded
            if (resp.timestamp != 0) {
                uint64_t now = rdtsc();
                uint64_t latency = now - resp.timestamp;
                ch->latency->record(latency);
            }
            ch->consumed.fetch_add(1, std::memory_order_relaxed);
        }

        // After draining, queue is empty
        // Loop back to uintr_wait() to sleep until next wakeup
    }

    // Cleanup
    close(ch->uintr_fd);
    uintr_unregister_handler(0);
}

// -------------------------------------------------------------
// Poller Thread (NEW: Edge-triggered UINTR notification)
// -------------------------------------------------------------
struct PollerState {
    bool was_empty;  // Poller-private state for each client
    int uipi_index;  // UINTR sender index for this client
};

void poller_thread_func(std::vector<ClientChannel*>& clients,
                       std::atomic<bool>& stop_flag) {
    size_t num_clients = clients.size();

    // Initialize per-client poller state
    std::vector<PollerState> poller_states(num_clients);

    // Register as UINTR sender for each client
    for (size_t i = 0; i < num_clients; ++i) {
        poller_states[i].was_empty = true;  // Assume initially empty

        // Wait for client to create uintr_fd
        while (clients[i]->uintr_fd < 0 && !stop_flag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (stop_flag.load()) return;

        poller_states[i].uipi_index = uintr_register_sender(clients[i]->uintr_fd, 0);
        if (poller_states[i].uipi_index < 0) {
            std::cerr << "[Poller] Failed to register sender for client " << i << "\n";
            return;
        }
    }

    // Main polling loop
    while (!stop_flag.load(std::memory_order_relaxed)) {
        // Round-robin scan all client response queues
        for (size_t i = 0; i < num_clients; ++i) {
            if (stop_flag.load()) break;

            // Check current queue state
            bool is_empty_now = clients[i]->resp_q->is_empty();

            // Detect empty → non-empty transition
            if (poller_states[i].was_empty && !is_empty_now) {
                // Send UINTR to wake up client response thread
                _senduipi(poller_states[i].uipi_index);
            }

            // Update state
            poller_states[i].was_empty = is_empty_now;
        }
    }

    // Cleanup: unregister all senders
    for (size_t i = 0; i < num_clients; ++i) {
        if (poller_states[i].uipi_index >= 0) {
            uintr_unregister_sender(poller_states[i].uipi_index, 0);
        }
    }
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <num_clients> <queue_size> <num_workers> <duration_sec>\n";
        return 1;
    }

    int num_clients = std::atoi(argv[1]);
    int queue_size = std::atoi(argv[2]);
    int num_workers = std::atoi(argv[3]);
    int duration = std::atoi(argv[4]);
    int sample_interval = 1000;  // Sample 1/1000 requests

    if (num_clients <= 0 || queue_size <= 0 || num_workers <= 0 || duration <= 0) {
        std::cerr << "Invalid arguments\n";
        return 1;
    }

    double cpu_freq_ghz = estimate_cpu_freq_ghz();
    std::cout << "Estimated CPU frequency: " << cpu_freq_ghz << " GHz\n";

    // Create client channels
    std::vector<ClientChannel*> clients;
    for (int i = 0; i < num_clients; ++i) {
        clients.push_back(new ClientChannel(queue_size, sample_interval));
    }

    // Create workers
    std::vector<Worker*> workers;
    for (int i = 0; i < num_workers; ++i) {
        workers.push_back(new Worker());
    }

    std::atomic<bool> stop_flag{false};
    uint64_t total_forwarded = 0;

    // Launch worker threads
    std::vector<std::thread> worker_threads;
    for (int i = 0; i < num_workers; ++i) {
        worker_threads.emplace_back(worker_thread_func, workers[i], std::ref(stop_flag));
        pin_thread_to_cpu(worker_threads.back(), 2 * num_clients + 1 + i);  // Pin workers after clients
    }

    // Launch synchronizer thread
    std::thread sync_thread(synchronizer_thread_func, std::ref(clients), std::ref(workers),
                           std::ref(stop_flag), std::ref(total_forwarded));
    pin_thread_to_cpu(sync_thread, 0);  // Pin synchronizer to CPU 0

    // Launch poller thread
    std::thread poller_thread(poller_thread_func, std::ref(clients), std::ref(stop_flag));
    pin_thread_to_cpu(poller_thread, 2 * num_clients + num_workers + 1);  // Pin poller after workers

    // Launch client threads
    std::vector<std::thread> client_request_threads;
    std::vector<std::thread> client_response_threads;

    for (int i = 0; i < num_clients; ++i) {
        // Request thread
        client_request_threads.emplace_back(client_request_thread_func, clients[i], i, std::ref(stop_flag));
        pin_thread_to_cpu(client_request_threads.back(), 1 + i);

        // Response thread (UINTR-based)
        client_response_threads.emplace_back(client_response_thread_func, clients[i], i,
                                            std::ref(stop_flag), cpu_freq_ghz);
        pin_thread_to_cpu(client_response_threads.back(), 1 + num_clients + i);
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    stop_flag.store(true);

    // Join all threads
    sync_thread.join();
    poller_thread.join();
    for (auto& t : worker_threads) t.join();
    for (auto& t : client_request_threads) t.join();
    for (auto& t : client_response_threads) t.join();

    // Print final statistics
    std::cout << "\n=== Final Statistics ===\n";
    std::cout << "Synchronizer forwarded total = " << total_forwarded << "\n";

    uint64_t total_produced = 0;
    uint64_t total_consumed = 0;
    for (int i = 0; i < num_clients; ++i) {
        total_produced += clients[i]->produced.load();
        total_consumed += clients[i]->consumed.load();
    }

    std::cout << "Processed total = " << total_consumed << "\n";
    std::cout << "Average Throughput = " << (double)total_consumed / duration << " req/sec\n";

    // Print per-client latency statistics
    std::cout << "\n=== Per-Client Latency ===\n";
    for (int i = 0; i < num_clients; ++i) {
        clients[i]->latency->print_statistics(cpu_freq_ghz, i);
    }

    // Cleanup
    for (auto& ch : clients) delete ch;
    for (auto& w : workers) delete w;

    return 0;
}
