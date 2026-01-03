# Baseline Test Infrastructure - Restoration Complete ✅

## Task Summary

Successfully restored all baseline testing and plotting infrastructure using the working UINTR poller scripts as templates, as requested.

## What Was Done

### 1. test.sh - Complete Rewrite ✅

**Major changes**:
- ✅ Changed program from `cxl_test` → `baseline`
- ✅ Added CPU frequency locking at 2 GHz (matches uintrpoller)
- ✅ Fixed CSV parsing to handle `=` signs and scientific notation
- ✅ Added latency extraction per client (grep -A 4)
- ✅ Added automatic plot generation (all 4 scripts)
- ✅ Changed output files to use `baseline_` prefix

**Key fixes**:
```bash
# OLD (broken):
./cxl_test "$clients" "$qsize"
throughput=$(echo "$output" | grep "Average Throughput" | awk '{print $3}')  # Gets "="

# NEW (working):
./baseline "$clients" "$qsize" "$SAMPLE_INTERVAL"
throughput=$(echo "$output" | grep "Average Throughput" | awk -F'=' '{print $2}' | awk '{print $1}')
```

### 2. plot.py - Updated ✅

**Changes**:
- Fixed CSV file name: `cxl_test_results.csv` → `baseline_test_results.csv`
- Added "Baseline:" prefix to title

### 3. plot_cpu_utilization.py - Updated ✅

**Changes**:
- Added "Baseline:" prefix to title for clarity

### 4. plot_throughput_over_time.py - Updated ✅

**Changes**:
- Added "Baseline:" prefix to title for clarity

### 5. plot_latency_vs_clients.py - Created NEW ✅

**Features**:
- Reads `latency_all_runs.csv`
- Groups by number of clients
- Plots 3 grouped bars (Avg, p50, p99)
- Converts nanoseconds → microseconds
- Saves to `latency_vs_clients.png`

## Infrastructure Comparison

Both baseline and UINTR poller now have **identical** testing capabilities:

| Feature | Baseline | UINTR Poller |
|---------|----------|--------------|
| **Test script** | ✅ test.sh | ✅ test_uintrpoller.sh |
| **CPU freq locking** | ✅ 2 GHz | ✅ 2 GHz |
| **CSV parsing** | ✅ Fixed | ✅ Fixed |
| **Latency extraction** | ✅ Yes | ✅ Yes |
| **Auto-plotting** | ✅ Yes | ✅ Yes |
| **Throughput plot** | ✅ plot.py | ✅ plot_uintrpoller.py |
| **CPU utilization** | ✅ plot_cpu_utilization.py | ✅ plot_uintrpoller_cpu_utilization.py |
| **Throughput/time** | ✅ plot_throughput_over_time.py | ✅ plot_uintrpoller_throughput_over_time.py |
| **Latency plot** | ✅ plot_latency_vs_clients.py | ✅ plot_uintrpoller_latency_vs_clients.py |

## Output Files

After running `./test.sh`, you'll get:

### CSV/Log Files
```
baseline_test_results.csv          # Summary: clients, qsize, run, total_requests, throughput
cpu0_all_runs.log                  # CPU0: time_sec, clients, run, cpu_usr, cpu_sys, cpu_idle
cpu0_freq_all_runs.log             # Frequency verification
throughput_all_runs.csv            # Per-second: time_sec, clients, run, throughput
latency_all_runs.csv               # Latency: clients, qsize, run, client_id, avg_ns, p50_ns, p99_ns
```

### PNG Plots (auto-generated)
```
throughput_vs_clients_bar.png      # Throughput bar chart
cpu0_utilization_all_runs.png      # CPU utilization time series
throughput_over_time.png           # Per-second throughput
latency_vs_clients.png             # NEW - Latency comparison (Avg, p50, p99)
```

## Usage

### Run Baseline Tests

```bash
./test.sh
```

This will:
1. Compile baseline
2. Lock CPU frequency at 2 GHz
3. Run tests for all client configurations (1, 2, 4, 8, 16, 32, 64)
4. Collect 5 CSV/log files
5. **Automatically generate all 4 plots**
6. Restore CPU frequency settings

**No manual plotting needed!**

### Run UINTR Poller Tests

```bash
./test_uintrpoller.sh
```

### Compare Results

```bash
# Test baseline
./test.sh
./backup_results.sh  # Backup as results_backup_YYYYMMDD_HHMMSS/

# Test UINTR poller
./test_uintrpoller.sh
./backup_results.sh  # Backup with new timestamp

# Compare plots side-by-side
# - throughput_vs_clients_bar.png vs uintrpoller_throughput_vs_clients_bar.png
# - latency_vs_clients.png vs uintrpoller_latency_vs_clients.png
```

## Files NOT Changed ✅

As requested:
- **baseline.cpp** - Left untouched (confirmed correct and latest)
- **Makefile** - No changes needed
- **backup_results.sh** - Works for both baseline and uintrpoller

## Test Configuration

**test.sh** settings:
- **Clients**: 1, 2, 4, 8, 16, 32, 64
- **Queue size**: 1024
- **Sample interval**: 1000
- **Duration**: 60 seconds (hardcoded in baseline.cpp)
- **Runs**: 3 per configuration
- **CPU frequency**: Locked at 2 GHz

## Verification Checklist

✅ test.sh restored and calls correct program (`baseline`)
✅ CSV parsing fixed (handles `=` and scientific notation)
✅ CPU frequency locking added (2 GHz)
✅ Latency extraction added (per client)
✅ Auto-plotting added (all 4 scripts)
✅ plot.py updated (correct CSV file, "Baseline:" title)
✅ plot_cpu_utilization.py updated ("Baseline:" title)
✅ plot_throughput_over_time.py updated ("Baseline:" title)
✅ plot_latency_vs_clients.py created (NEW)
✅ baseline.cpp NOT touched (as requested)
✅ Both baseline and UINTR poller have identical infrastructure

## Ready to Test

All baseline testing infrastructure is now fully restored and ready to use! 🎉

You can now:
1. Run `./test.sh` to benchmark baseline
2. Run `./test_uintrpoller.sh` to benchmark UINTR poller
3. Compare results side-by-side
4. Use `./backup_results.sh` to archive results before next run
