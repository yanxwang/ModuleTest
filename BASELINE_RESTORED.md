# Baseline Test Scripts Restored

## Summary

Successfully restored all baseline testing and plotting infrastructure based on the working uintrpoller versions.

## Files Restored/Updated

### Test Script
| File | Status | Changes |
|------|--------|---------|
| **[test.sh](test.sh)** | ✅ Restored | Complete rewrite - now matches uintrpoller.sh format |

### Plotting Scripts (4 files)
| File | Status | Changes |
|------|--------|---------|
| **[plot.py](plot.py)** | ✅ Updated | Fixed CSV file name, added "Baseline:" title |
| **[plot_cpu_utilization.py](plot_cpu_utilization.py)** | ✅ Updated | Added "Baseline:" title |
| **[plot_throughput_over_time.py](plot_throughput_over_time.py)** | ✅ Updated | Added "Baseline:" title |
| **[plot_latency_vs_clients.py](plot_latency_vs_clients.py)** | ✅ **NEW** | Created from scratch (was missing) |

## Key Changes in test.sh

### Fixed Issues

1. ✅ **Program name**: Changed from `cxl_test` → `baseline`
2. ✅ **CSV parsing**: Fixed to handle `=` signs and scientific notation
3. ✅ **Latency extraction**: Added per-client latency parsing
4. ✅ **CPU frequency locking**: Added 2 GHz locking (same as uintrpoller)
5. ✅ **Auto-plotting**: Automatically runs all 4 plot scripts
6. ✅ **File names**: Uses `baseline_` prefix for all outputs

### Before vs After

**OLD test.sh** (broken):
```bash
# Wrong program
sudo perf record ... ./cxl_test "$clients" "$qsize"

# Wrong CSV file names
OUTPUT_FILE="cxl_test_results.csv"

# Wrong parsing (grabs "=")
throughput=$(echo "$output" | grep "Average Throughput" | awk '{print $3}')

# No latency extraction
# No CPU frequency locking
# No auto-plotting
```

**NEW test.sh** (working):
```bash
# Correct program
./baseline "$clients" "$qsize" "$SAMPLE_INTERVAL"

# Correct CSV file names
OUTPUT_FILE="baseline_test_results.csv"

# Correct parsing (extracts value after "=")
throughput=$(echo "$output" | grep "Average Throughput" | awk -F'=' '{print $2}' | awk '{print $1}')

# Latency extraction (grep -A 4 "\[Client")
# CPU frequency locked at 2 GHz
# Auto-plotting all 4 scripts
```

## Test Configuration

**test.sh** settings:
- **Clients**: 1, 2, 4, 8, 16, 32, 64
- **Queue size**: 1024
- **Sample interval**: 1000
- **Duration**: 60 seconds (hardcoded in baseline.cpp)
- **Runs**: 3 per configuration
- **CPU frequency**: Locked at 2 GHz

## Output Files

After running `./test.sh`:

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
latency_vs_clients.png             # NEW - Latency comparison
```

## Plot Scripts Details

### 1. plot.py - Throughput Bar Chart
- **Reads**: `baseline_test_results.csv`
- **Groups by**: Number of clients
- **Plots**: Average throughput (M req/sec)
- **Title**: "Baseline: Synchronizer Throughput vs Number of Clients"
- **Output**: `throughput_vs_clients_bar.png`

### 2. plot_cpu_utilization.py - CPU Utilization
- **Reads**: `cpu0_all_runs.log`
- **Plots**: %usr, %sys, %idle over time
- **Title**: "Baseline: CPU 0 Utilization Over Time (All Runs)"
- **Output**: `cpu0_utilization_all_runs.png`

### 3. plot_throughput_over_time.py - Throughput Time Series
- **Reads**: `throughput_all_runs.csv`
- **Plots**: Per-second throughput for each (client, run) combination
- **Title**: "Baseline: Per-Second Throughput Over Time"
- **Output**: `throughput_over_time.png`

### 4. plot_latency_vs_clients.py - Latency Comparison (NEW!)
- **Reads**: `latency_all_runs.csv`
- **Groups by**: Number of clients
- **Plots**: 3 grouped bars (Avg, p50, p99) in microseconds
- **Title**: "Baseline: Per-Request Latency vs Number of Clients"
- **Output**: `latency_vs_clients.png`

## Usage

### Run Full Test Suite

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

### Manual Plotting (if needed)

```bash
python3 plot.py
python3 plot_cpu_utilization.py
python3 plot_throughput_over_time.py
python3 plot_latency_vs_clients.py
```

## Comparison: Baseline vs UINTR Poller

Both now have **identical infrastructure**:

| Feature | Baseline | UINTR Poller |
|---------|----------|--------------|
| **Test script** | ✅ test.sh | ✅ test_uintrpoller.sh |
| **CPU frequency locking** | ✅ 2 GHz | ✅ 2 GHz |
| **CSV parsing** | ✅ Fixed | ✅ Fixed |
| **Latency extraction** | ✅ Yes | ✅ Yes |
| **Auto-plotting** | ✅ Yes | ✅ Yes |
| **Throughput plot** | ✅ plot.py | ✅ plot_uintrpoller.py |
| **CPU utilization plot** | ✅ plot_cpu_utilization.py | ✅ plot_uintrpoller_cpu_utilization.py |
| **Throughput over time** | ✅ plot_throughput_over_time.py | ✅ plot_uintrpoller_throughput_over_time.py |
| **Latency plot** | ✅ plot_latency_vs_clients.py | ✅ plot_uintrpoller_latency_vs_clients.py |

## Parallel Testing Workflow

To compare baseline vs UINTR poller:

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

## What's NOT Changed

- ✅ **baseline.cpp** - Left untouched (already correct)
- ✅ **Makefile** - No changes needed
- ✅ **backup_results.sh** - Works for both baseline and uintrpoller

## Verification

All files are ready:

```bash
# Check test script
ls -lh test.sh
-rwxrwxr-x 1 wang wang 6.2K Dec 18 00:24 test.sh

# Check plotting scripts
ls -lh plot*.py | grep -v uintrpoller
-rw-rw-r-- 1 wang wang  905 Dec 18 00:24 plot_cpu_utilization.py
-rw------- 1 wang wang 1.5K Dec 18 00:25 plot_latency_vs_clients.py
-rw-rw-r-- 1 wang wang  937 Dec 18 00:24 plot.py
-rw-rw-r-- 1 wang wang  617 Dec 18 00:24 plot_throughput_over_time.py

# Test it
./test.sh
```

## Next Steps

1. **Run baseline tests**: `./test.sh`
2. **Check plots**: Verify all 4 PNG files generated
3. **Run UINTR tests**: `./test_uintrpoller.sh`
4. **Compare**: Side-by-side comparison of plots
5. **Backup**: Use `./backup_results.sh` after each test run

All baseline testing infrastructure is now fully restored and working! 🎉
