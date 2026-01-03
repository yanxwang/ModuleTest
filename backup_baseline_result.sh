#!/bin/bash

# Backup script for baseline test results
# Creates timestamped backup of all CSV/log files and plots

# Generate timestamp for backup folder
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BACKUP_DIR="baseline_results_backup_${TIMESTAMP}"

# Create backup directory
mkdir -p "$BACKUP_DIR"

echo "Backing up test results to $BACKUP_DIR/"

# List of files to backup
FILES_TO_BACKUP=(
    "baseline_test_results.csv"
    "cpu0_all_runs.log"
    "cpu0_freq_all_runs.log"
    "throughput_all_runs.csv"
    "latency_all_runs.csv"
    "throughput_vs_clients_bar.png"
    "cpu0_utilization_all_runs.png"
    "throughput_over_time.png"
    "latency_vs_clients.png"
)

# Copy files if they exist
COPIED=0
for file in "${FILES_TO_BACKUP[@]}"; do
    if [ -f "$file" ]; then
        cp "$file" "$BACKUP_DIR/"
        echo "  ✓ Backed up: $file"
        COPIED=$((COPIED + 1))
    else
        echo "  ⚠ Not found: $file (skipping)"
    fi
done

# Create a metadata file with test parameters
cat > "$BACKUP_DIR/metadata.txt" <<EOF
Backup Timestamp: $(date)
Hostname: $(hostname)
Working Directory: $(pwd)

Files Backed Up: $COPIED / ${#FILES_TO_BACKUP[@]}

Test Configuration:
- CPU Frequency: 2000 MHz (locked)
- Test Duration: 60 seconds per run
- Queue Size: 1024
- Sample Interval: 1000
- Clients Tested: 1, 2, 4, 8, 16, 32, 64
- Runs per config: 3

System Info:
$(lscpu | grep "Model name:")
$(lscpu | grep "CPU(s):")
$(lscpu | grep "Thread(s) per core:")
$(lscpu | grep "NUMA node(s):")
EOF

echo ""
echo "Backup complete: $BACKUP_DIR/"
echo "Total files backed up: $COPIED"
echo ""
echo "To restore a backup:"
echo "  cp ${BACKUP_DIR}/* ."
echo ""
echo "To list all backups:"
echo "  ls -lhd results_backup_*/"
