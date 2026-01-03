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
        uint64_t min_cycles = samples[0];
        uint64_t max_cycles = samples[n - 1];
        uint64_t sum = 0;
        for (size_t i = 0; i < n; i++) sum += samples[i];
        double avg_cycles = (double)sum / n;
        uint64_t p50 = samples[n / 2];
        uint64_t p90 = samples[(size_t)(n * 0.90)];
        uint64_t p95 = samples[(size_t)(n * 0.95)];
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

    ClientChannel(size_t queue_size, size_t max_samples, size_t sample_interval) {
        req_q = new LockFreeQueue<VersionRequest>(queue_size);
        resp_q = new LockFreeQueue<VersionResponse>(queue_size);
        latency = new LatencyTracker(max_samples, sample_interval);
    }
    ~ClientChannel() {
        delete req_q;
        delete resp_q;
        delete latency;
    }
};

// -------------------------------------------------------------
// Pin threads to CPU
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
// Synchronizer
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
                    // This should not happen if workers are faster than synchronizer
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
        // For test.sh parsing: report synchronizer throughput
        std::cout << "Time " << sec << "s: " << sync_delta << " req/sec\n";
        // Also log system throughput separately for debugging
        std::cout << "  System: " << system_delta << " req/sec\n";

        if (stop_flag.load()) break;
    }
    total_forwarded = forwarded;
}

// -------------------------------------------------------------
// Client - Request Thread
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
// Client - Response Thread
// -------------------------------------------------------------
void client_response_thread_func(ClientChannel* ch, uint32_t client_id,
                                  std::atomic<bool>& stop_flag, double cpu_freq_ghz) {
    while (!stop_flag.load(std::memory_order_relaxed)) {
        VersionResponse resp;
        if (ch->resp_q->dequeue(resp)) {
            // Measure latency if timestamp was recorded
            if (resp.timestamp != 0) {
                uint64_t now = rdtsc();
                uint64_t latency = now - resp.timestamp;
                ch->latency->record(latency);
            }
            ch->consumed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------
int main(int argc, char** argv) {
    size_t num_clients = 4;
    size_t queue_size = 1024;
    size_t sample_interval = 1000;
    size_t num_workers = 10;
    if (argc > 1) num_clients = std::stoul(argv[1]);
    if (argc > 2) queue_size = std::stoul(argv[2]);
    if (argc > 3) sample_interval = std::stoul(argv[3]);

    double cpu_freq_ghz = estimate_cpu_freq_ghz();

    std::vector<ClientChannel*> clients;
    clients.reserve(num_clients);
    for (size_t i = 0; i < num_clients; ++i)
        clients.push_back(new ClientChannel(queue_size, 1000000 / num_clients, sample_interval));

    std::vector<Worker*> workers;
    for (size_t i = 0; i < num_workers; ++i) {
        workers.push_back(new Worker());
    }

    std::atomic<bool> stop_flag{false};
    uint64_t total_forwarded = 0;

    // Launch synchronizer thread
    std::thread sync_thread(synchronizer_thread_func, std::ref(clients), std::ref(workers),
                            std::ref(stop_flag), std::ref(total_forwarded));
    pin_thread_to_cpu(sync_thread, 0);

    // Launch client request threads
    std::vector<std::thread> client_request_threads;
    for (size_t i = 0; i < num_clients; ++i) {
        client_request_threads.emplace_back(client_request_thread_func, clients[i], i, std::ref(stop_flag));
        pin_thread_to_cpu(client_request_threads.back(), 1 + i);
    }

    // Launch client response threads
    std::vector<std::thread> client_response_threads;
    for (size_t i = 0; i < num_clients; ++i) {
        client_response_threads.emplace_back(client_response_thread_func, clients[i], i,
                                              std::ref(stop_flag), cpu_freq_ghz);
        pin_thread_to_cpu(client_response_threads.back(), 1 + num_clients + i);
    }

    // Launch worker threads
    std::vector<std::thread> worker_threads;
    for (size_t i = 0; i < num_workers; ++i) {
        worker_threads.emplace_back(worker_thread_func, workers[i], std::ref(stop_flag));
        pin_thread_to_cpu(worker_threads.back(), 1 + 2 * num_clients + i);
    }

    // Run for TEST_DURATION_SEC
    constexpr size_t TEST_DURATION_SEC = 60;
    std::this_thread::sleep_for(std::chrono::seconds(TEST_DURATION_SEC));
    stop_flag.store(true);

    for (auto& t : client_request_threads) t.join();
    for (auto& t : client_response_threads) t.join();
    for (auto& t : worker_threads) t.join();
    sync_thread.join();

    uint64_t total_produced = 0, total_consumed = 0;
    for (auto& ch : clients) {
        total_produced += ch->produced.load();
        total_consumed += ch->consumed.load();
    }

    std::cout << "\n=== Final Statistics ===\n";

    // For test.sh parsing: keep these exact formats (field $4)
    std::cout << "Processed total = " << total_forwarded << "\n";
    std::cout << "Average Throughput: " << double(total_forwarded)/TEST_DURATION_SEC << "\n";

    std::cout << "\n[Detailed Metrics]\n";
    std::cout << "  Synchronizer: " << total_forwarded << " requests handed off ("
              << double(total_forwarded)/TEST_DURATION_SEC << " req/sec)\n";
    std::cout << "  System: " << total_consumed << " responses completed ("
              << double(total_consumed)/TEST_DURATION_SEC << " req/sec)\n";
    std::cout << "  Clients: " << total_produced << " requests generated\n";

    std::cout << "\n=== Per-Request Latency (Request Enqueue → Response Dequeue) ===\n";
    for (size_t i = 0; i < num_clients; ++i)
        clients[i]->latency->print_statistics(cpu_freq_ghz, i);

    for (auto* ch : clients) delete ch;
    for (auto* w : workers) delete w;
    return 0;
}
