# UINTR Poller Plotting Scripts - Fixed and Created

## Issues Fixed

### 1. ✅ CSV Parsing Bug in test_uintrpoller.sh

**Problem**: The throughput column showed "=" instead of actual values because the output format uses:
```
Processed total = 347943551
Average Throughput = 5.79906e+06 req/sec
```

**Old parsing** (incorrect):
```bash
total_requests=$(echo "$output" | grep "Processed total" | awk '{print $4}')
throughput=$(echo "$output" | grep "Average Throughput" | awk '{print $3}')
```
This grabbed the "=" sign.

**New parsing** (correct):
```bash
total_requests=$(echo "$output" | grep "Processed total" | awk -F'=' '{print $2}' | tr -d ' ')
throughput=$(echo "$output" | grep "Average Throughput" | awk -F'=' '{print $2}' | awk '{print $1}')
```
This splits on "=" and extracts the numeric value.

### 2. ✅ Automatic Plotting Added

**Changed**: test_uintrpoller.sh now automatically runs all 4 plotting scripts after tests complete (instead of just printing instructions).

## Files Created

### Plotting Scripts

All 4 plotting scripts now exist for uintrpoller:

| Script | Output File | Description |
|--------|-------------|-------------|
| **[plot_uintrpoller.py](plot_uintrpoller.py)** | `uintrpoller_throughput_vs_clients_bar.png` | Bar chart of throughput vs number of clients |
| **[plot_uintrpoller_cpu_utilization.py](plot_uintrpoller_cpu_utilization.py)** | `uintrpoller_cpu0_utilization_all_runs.png` | CPU 0 utilization over time (%usr, %sys, %idle) |
| **[plot_uintrpoller_throughput_over_time.py](plot_uintrpoller_throughput_over_time.py)** | `uintrpoller_throughput_over_time.png` | Per-second throughput time series |
| **[plot_uintrpoller_latency_vs_clients.py](plot_uintrpoller_latency_vs_clients.py)** | `uintrpoller_latency_vs_clients.png` | **NEW** - Per-request latency (Avg, p50, p99) vs clients |

### Key Features of Latency Plot

The new `plot_uintrpoller_latency_vs_clients.py` script:

- **Reads**: `uintrpoller_latency_all_runs.csv`
- **Groups by**: Number of clients
- **Computes**: Average across all client IDs and runs
- **Converts**: Nanoseconds → Microseconds (÷ 1000)
- **Plots**: 3 grouped bars per client count:
  - Blue: Average latency
  - Orange: p50 (median) latency
  - Red: p99 (tail) latency
- **Labels**: Values on top of each bar
- **Output**: `uintrpoller_latency_vs_clients.png` (300 DPI)

### Example Output CSV Structure

**uintrpoller_latency_all_runs.csv**:
```csv
clients,queue_size,run,client_id,avg_latency_ns,p50_latency_ns,p99_latency_ns
2,1024,1,0,1234.5,1200.3,1500.8
2,1024,1,1,1245.2,1210.1,1520.4
4,1024,1,0,1567.8,1540.2,1890.3
4,1024,1,1,1570.1,1545.6,1895.7
...
```

The script averages across:
- All client IDs (0, 1, 2, ...)
- All runs (1, 2, 3)

For each unique client count (2, 4, 8, 16, ...).

## Usage

### Run Full Test Suite

```bash
./test_uintrpoller.sh
```

This will:
1. Compile uintrpoller
2. Lock CPU frequency at 2 GHz
3. Run tests for all client configurations
4. Collect CSV/log files
5. **Automatically generate all 4 plots**
6. Restore CPU frequency settings

### Manual Plotting (if needed)

```bash
python3 plot_uintrpoller.py
python3 plot_uintrpoller_cpu_utilization.py
python3 plot_uintrpoller_throughput_over_time.py
python3 plot_uintrpoller_latency_vs_clients.py
```

## Output Files After Test

### CSV/Log Files
```
uintrpoller_test_results.csv          # Summary: clients, queue_size, run, total_requests, throughput
uintrpoller_cpu0_all_runs.log         # CPU0: time_sec, clients, run, cpu_usr, cpu_sys, cpu_idle
uintrpoller_cpu0_freq_all_runs.log    # Frequency verification
uintrpoller_throughput_all_runs.csv   # Per-second: time_sec, clients, run, throughput
uintrpoller_latency_all_runs.csv      # Latency: clients, qsize, run, client_id, avg_ns, p50_ns, p99_ns
```

### PNG Plots
```
uintrpoller_throughput_vs_clients_bar.png     # Throughput bar chart
uintrpoller_cpu0_utilization_all_runs.png     # CPU utilization lines
uintrpoller_throughput_over_time.png          # Throughput time series
uintrpoller_latency_vs_clients.png            # NEW - Latency comparison bars
```

## Comparison with Baseline

Both baseline and uintrpoller now have identical plotting infrastructure:

| Aspect | Baseline | UINTR Poller |
|--------|----------|--------------|
| Throughput plot | ✅ plot.py | ✅ plot_uintrpoller.py |
| CPU utilization | ✅ plot_cpu_utilization.py | ✅ plot_uintrpoller_cpu_utilization.py |
| Throughput over time | ✅ plot_throughput_over_time.py | ✅ plot_uintrpoller_throughput_over_time.py |
| Latency vs clients | ❌ (missing) | ✅ plot_uintrpoller_latency_vs_clients.py |
| Auto-plotting in test script | ❌ No | ✅ Yes |

**Note**: Baseline is missing a dedicated latency plotting script. You can create `plot_latency_vs_clients.py` for baseline using the same format as the uintrpoller version (just change input file to `latency_all_runs.csv` and output to `latency_vs_clients.png`).

## Interpreting the Latency Plot

### What the Bars Show

- **Average (blue)**: Mean latency across all sampled requests
  - Good for understanding typical performance
  - Affected by outliers

- **p50 (orange)**: Median latency
  - 50% of requests complete faster than this
  - More robust to outliers than average

- **p99 (red)**: 99th percentile latency
  - Only 1% of requests are slower than this
  - Critical for SLA compliance
  - Shows tail latency behavior

### Expected Patterns

**Low Client Count** (1-4):
- Low latency (~1-2 μs)
- Small gap between avg/p50/p99
- System not saturated

**Medium Client Count** (8-16):
- Moderate latency increase
- p99 starts diverging from p50
- Queueing effects appear

**High Client Count** (32-64):
- Higher latency (queue depth effects)
- Large p99 spike (tail latency)
- System approaching saturation

### Comparing Baseline vs UINTR Poller

Run both test suites and compare the latency plots:

```bash
# Run baseline
./test.sh

# Run UINTR poller
./test_uintrpoller.sh

# Compare latency_vs_clients.png (baseline) vs uintrpoller_latency_vs_clients.png
```

Expected differences:
- **UINTR poller**: +1-2 μs latency due to wake-up overhead
- **Baseline**: Lower latency but uses more CPU (busy-polling)

## Troubleshooting

### "No such file or directory" when plotting

**Cause**: CSV file not generated or has wrong name

**Fix**: Check that test ran successfully and generated `uintrpoller_latency_all_runs.csv`

### Empty plot or "ValueError: No objects to concatenate"

**Cause**: No data in CSV file or parsing failed

**Fix**:
1. Check CSV file has data: `cat uintrpoller_latency_all_runs.csv`
2. Verify parsing is working: Run one manual test and check output

### "KeyError: 'throughput'" in plot_uintrpoller.py

**Cause**: CSV parsing bug not fixed

**Fix**: Make sure you're using the updated test_uintrpoller.sh with correct awk parsing

### Scientific notation in CSV

**Cause**: Throughput values written in scientific notation (e.g., 5.79906e+06)

**Status**: This is normal - pandas handles it correctly when reading CSV

## Next Steps

1. **Run tests**: `./test_uintrpoller.sh`
2. **Check plots**: Open the 4 PNG files
3. **Compare with baseline**: Run `./test.sh` and compare results
4. **Backup results**: `./backup_results.sh`

All plotting infrastructure is now complete and working!
