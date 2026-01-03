#!/bin/bash

make clean
make baseline
RUNS=3
CLIENTS_LIST=(1 2 4 8 16 32 64)
QUEUE_SIZES=(1024)
SAMPLE_INTERVAL=1000
OUTPUT_FILE="baseline_test_results.csv"
CPU_LOG_FILE="cpu0_all_runs.log"
THROUGHPUT_FILE="throughput_all_runs.csv"
FREQ_LOG_FILE="cpu0_freq_all_runs.log"
LATENCY_FILE="latency_all_runs.csv"

# Set CPU frequency to 2000 MHz for consistent performance
echo "Setting CPU frequency to 2000 MHz..."

# Set performance governor
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu > /dev/null 2>&1
done

# Set max frequency to 2000 MHz
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq; do
    echo 2000000 | sudo tee $cpu > /dev/null 2>&1
done

# Set min frequency to 2000 MHz to lock at 2 GHz
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq; do
    echo 2000000 | sudo tee $cpu > /dev/null 2>&1
done

# Disable turbo boost
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null 2>&1

# Verify frequency settings
sleep 1  # Wait for settings to take effect
CURRENT_FREQ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null | awk '{printf "%.0f\n", $1/1000}')
MIN_FREQ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq 2>/dev/null | awk '{printf "%.0f\n", $1/1000}')
MAX_FREQ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null | awk '{printf "%.0f\n", $1/1000}')
echo "CPU0 frequency locked: min=${MIN_FREQ} MHz, max=${MAX_FREQ} MHz, current=${CURRENT_FREQ} MHz"

# Prepare output files
echo "clients,queue_size,run,total_requests,throughput" > "$OUTPUT_FILE"
echo "time_sec,clients,run,cpu_usr,cpu_sys,cpu_idle" > "$CPU_LOG_FILE"
echo "time_sec,clients,run,throughput" > "$THROUGHPUT_FILE"
echo "clients,queue_size,run,cpu0_freq_MHz" > "$FREQ_LOG_FILE"
echo "clients,queue_size,run,client_id,avg_latency_ns,p50_latency_ns,p99_latency_ns" > "$LATENCY_FILE"

TIME_COUNTER=0

for clients in "${CLIENTS_LIST[@]}"; do
    for qsize in "${QUEUE_SIZES[@]}"; do
        for run in $(seq 1 $RUNS); do
            echo "Running: clients=$clients, queue_size=$qsize, run=$run"

            # Start mpstat for CPU 0 in background
            mpstat -P 0 1 > "cpu0_temp.log" &
            MPSTAT_PID=$!

            # Run baseline test (no perf, no sudo needed)
            ./baseline "$clients" "$qsize" "$SAMPLE_INTERVAL" > "test_output.log" 2>&1

            # Stop mpstat
            kill $MPSTAT_PID
            wait $MPSTAT_PID 2>/dev/null

            # Extract output for parsing
            output=$(cat test_output.log)

            # Extract performance numbers (end summary)
            total_requests=$(echo "$output" | grep "Processed total" | awk -F'=' '{print $2}' | tr -d ' ')
            throughput=$(echo "$output" | grep "Average Throughput" | awk -F'=' '{print $2}' | awk '{print $1}')

            # Extract per-second throughput lines
            echo "$output" | grep "Time " | while read -r line; do
                # Example line: "Time 1s: 12345 req/sec"
                t=$(echo "$line" | awk '{print $2}' | tr -d 's:')
                thr=$(echo "$line" | awk '{print $3}')
                echo "$t,$clients,$run,$thr" >> "$THROUGHPUT_FILE"
            done

            # Extract CPU 0 utilization per second and append to single file
            awk -v c="$clients" -v r="$run" -v t_start=$TIME_COUNTER \
                '/^[0-9]/ {print t_start+NR-1 "," c "," r "," $4 "," $6 "," $13}' cpu0_temp.log \
                >> "$CPU_LOG_FILE"

            # Check CPU0 current frequency in MHz
            cpu_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null | awk '{printf "%.0f\n", $1/1000}')
            if [ -z "$cpu_freq" ]; then
                cpu_freq="N/A"
            fi
            echo "CPU0 frequency after run: ${cpu_freq} MHz"
            echo "$clients,$qsize,$run,$cpu_freq" >> "$FREQ_LOG_FILE"

            # Extract latency data per client
            echo "$output" | grep -A 4 "^\[Client" | while IFS= read -r line; do
                if [[ $line =~ ^\[Client\ ([0-9]+)\] ]]; then
                    client_id="${BASH_REMATCH[1]}"
                    read -r avg_line
                    read -r p50_line
                    read -r p90_line
                    read -r p99_line
                    avg_ns=$(echo "$avg_line" | awk '{print $2}')
                    p50_ns=$(echo "$p50_line" | awk '{print $2}')
                    p99_ns=$(echo "$p99_line" | awk '{print $2}')
                    echo "$clients,$qsize,$run,$client_id,$avg_ns,$p50_ns,$p99_ns" >> "$LATENCY_FILE"
                fi
            done

            echo "$clients,$qsize,$run,$total_requests,$throughput" >> "$OUTPUT_FILE"

            # Update global time counter
            TIME_COUNTER=$((TIME_COUNTER + $(wc -l < cpu0_temp.log) - 2))  # subtract header lines

            echo "Completed: clients=$clients, queue_size=$qsize, run=$run"
        done
    done
done

echo "All tests done. CSV summary: $OUTPUT_FILE"
echo "CPU 0 time series log: $CPU_LOG_FILE"
echo "CPU 0 frequency log: $FREQ_LOG_FILE"
echo "Per-second throughput log: $THROUGHPUT_FILE"
echo "Latency log: $LATENCY_FILE"

# Restore CPU frequency scaling to default
echo "Restoring CPU frequency settings..."

# Restore min frequency to 800 MHz
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq; do
    echo 800000 | sudo tee $cpu > /dev/null 2>&1
done

# Restore max frequency to 3000 MHz
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq; do
    echo 3000000 | sudo tee $cpu > /dev/null 2>&1
done

# Restore powersave governor
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo powersave | sudo tee $cpu > /dev/null 2>&1
done

# Re-enable turbo boost
echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null 2>&1

echo "CPU frequency settings restored."
echo ""
echo "Generating plots..."
python3 plot.py
python3 plot_cpu_utilization.py
python3 plot_throughput_over_time.py
python3 plot_latency_vs_clients.py
echo ""
echo "All plots generated!"
echo "  - throughput_vs_clients_bar.png"
echo "  - cpu0_utilization_all_runs.png"
echo "  - throughput_over_time.png"
echo "  - latency_vs_clients.png"
