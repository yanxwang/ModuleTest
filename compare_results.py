#!/usr/bin/env python3
"""
Compare baseline vs UINTR poller results from backup directories.
Generates comparison plots for throughput and latency.

Usage:
    python3 compare_results.py <baseline_dir> <uintrpoller_dir>

Example:
    python3 compare_results.py results_backup_20251217_005128 uintrpoller_results_backup_20251218_004925
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime

def load_throughput_data(baseline_dir, uintrpoller_dir):
    """Load throughput CSV files from both directories."""
    baseline_csv = os.path.join(baseline_dir, "baseline_test_results.csv")
    uintrpoller_csv = os.path.join(uintrpoller_dir, "uintrpoller_test_results.csv")

    if not os.path.exists(baseline_csv):
        raise FileNotFoundError(f"Baseline CSV not found: {baseline_csv}")
    if not os.path.exists(uintrpoller_csv):
        raise FileNotFoundError(f"UINTR poller CSV not found: {uintrpoller_csv}")

    baseline_df = pd.read_csv(baseline_csv)
    uintrpoller_df = pd.read_csv(uintrpoller_csv)

    return baseline_df, uintrpoller_df

def load_latency_data(baseline_dir, uintrpoller_dir):
    """Load latency CSV files from both directories."""
    baseline_csv = os.path.join(baseline_dir, "latency_all_runs.csv")
    uintrpoller_csv = os.path.join(uintrpoller_dir, "uintrpoller_latency_all_runs.csv")

    if not os.path.exists(baseline_csv):
        raise FileNotFoundError(f"Baseline latency CSV not found: {baseline_csv}")
    if not os.path.exists(uintrpoller_csv):
        raise FileNotFoundError(f"UINTR poller latency CSV not found: {uintrpoller_csv}")

    baseline_df = pd.read_csv(baseline_csv)
    uintrpoller_df = pd.read_csv(uintrpoller_csv)

    return baseline_df, uintrpoller_df

def plot_throughput_comparison(baseline_df, uintrpoller_df, output_file="comparison_throughput_vs_clients.png"):
    """Plot throughput comparison bar chart."""
    # Compute average throughput per number of clients
    baseline_avg = baseline_df.groupby("clients")["throughput"].mean() / 1e6  # M req/sec
    uintrpoller_avg = uintrpoller_df.groupby("clients")["throughput"].mean() / 1e6

    # Get all unique client counts (union of both datasets)
    all_clients = sorted(set(baseline_avg.index) | set(uintrpoller_avg.index))

    # Align data
    baseline_values = [baseline_avg.get(c, 0) for c in all_clients]
    uintrpoller_values = [uintrpoller_avg.get(c, 0) for c in all_clients]

    # Plot
    fig, ax = plt.subplots(figsize=(12, 6))

    x = np.arange(len(all_clients))
    width = 0.35

    bars1 = ax.bar(x - width/2, baseline_values, width, label='Baseline', color='steelblue', alpha=0.8)
    bars2 = ax.bar(x + width/2, uintrpoller_values, width, label='UINTR Poller', color='orange', alpha=0.8)

    # Add value labels on bars
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(bar.get_x() + bar.get_width()/2.0, height,
                       f'{height:.2f}', ha='center', va='bottom', fontsize=8)

    ax.set_xlabel("Number of Clients", fontsize=12)
    ax.set_ylabel("Average Throughput (Million req/sec)", fontsize=12)
    ax.set_title("Throughput Comparison: Baseline vs UINTR Poller", fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(all_clients)
    ax.legend(fontsize=10)
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"✓ Throughput comparison saved as {output_file}")

def plot_latency_comparison(baseline_df, uintrpoller_df, output_file="comparison_latency_vs_clients.png"):
    """Plot latency comparison with baseline vs UINTR poller side-by-side for avg, p50, p99."""
    # Group by clients and compute average latency across all clients and runs
    baseline_avg = baseline_df.groupby("clients")["avg_latency_ns"].mean() / 1000  # Convert to μs
    baseline_p50 = baseline_df.groupby("clients")["p50_latency_ns"].mean() / 1000
    baseline_p99 = baseline_df.groupby("clients")["p99_latency_ns"].mean() / 1000

    uintrpoller_avg = uintrpoller_df.groupby("clients")["avg_latency_ns"].mean() / 1000
    uintrpoller_p50 = uintrpoller_df.groupby("clients")["p50_latency_ns"].mean() / 1000
    uintrpoller_p99 = uintrpoller_df.groupby("clients")["p99_latency_ns"].mean() / 1000

    # Get all unique client counts
    all_clients = sorted(set(baseline_avg.index) | set(uintrpoller_avg.index))

    # Create three subplots for Avg, p50, and p99
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6))

    x = np.arange(len(all_clients))
    width = 0.35

    # Plot 1: Average Latency Comparison
    bars1_avg = ax1.bar(x - width/2, [baseline_avg.get(c, 0) for c in all_clients],
                        width, label='Baseline', color='steelblue', alpha=0.8)
    bars2_avg = ax1.bar(x + width/2, [uintrpoller_avg.get(c, 0) for c in all_clients],
                        width, label='UINTR Poller', color='orange', alpha=0.8)

    # Add value labels with smart formatting
    for bars in [bars1_avg, bars2_avg]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                # Format large numbers with 'k' suffix
                if height >= 1000:
                    label = f'{height/1000:.1f}k'
                else:
                    label = f'{height:.0f}'
                ax1.text(bar.get_x() + bar.get_width()/2.0, height,
                        label, ha='center', va='bottom', fontsize=6, rotation=45)

    ax1.set_xlabel("Number of Clients", fontsize=11)
    ax1.set_ylabel("Latency (microseconds)", fontsize=11)
    ax1.set_title("Average Latency Comparison", fontsize=12, fontweight='bold')
    ax1.set_xticks(x)
    ax1.set_xticklabels(all_clients)
    ax1.legend(fontsize=9)
    ax1.grid(axis='y', linestyle='--', alpha=0.7)

    # Plot 2: p50 Latency Comparison
    bars1_p50 = ax2.bar(x - width/2, [baseline_p50.get(c, 0) for c in all_clients],
                        width, label='Baseline', color='steelblue', alpha=0.8)
    bars2_p50 = ax2.bar(x + width/2, [uintrpoller_p50.get(c, 0) for c in all_clients],
                        width, label='UINTR Poller', color='orange', alpha=0.8)

    # Add value labels with smart formatting
    for bars in [bars1_p50, bars2_p50]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                # Format large numbers with 'k' suffix
                if height >= 1000:
                    label = f'{height/1000:.1f}k'
                else:
                    label = f'{height:.0f}'
                ax2.text(bar.get_x() + bar.get_width()/2.0, height,
                        label, ha='center', va='bottom', fontsize=6, rotation=45)

    ax2.set_xlabel("Number of Clients", fontsize=11)
    ax2.set_ylabel("Latency (microseconds)", fontsize=11)
    ax2.set_title("p50 (Median) Latency Comparison", fontsize=12, fontweight='bold')
    ax2.set_xticks(x)
    ax2.set_xticklabels(all_clients)
    ax2.legend(fontsize=9)
    ax2.grid(axis='y', linestyle='--', alpha=0.7)

    # Plot 3: p99 Latency Comparison
    bars1_p99 = ax3.bar(x - width/2, [baseline_p99.get(c, 0) for c in all_clients],
                        width, label='Baseline', color='steelblue', alpha=0.8)
    bars2_p99 = ax3.bar(x + width/2, [uintrpoller_p99.get(c, 0) for c in all_clients],
                        width, label='UINTR Poller', color='orange', alpha=0.8)

    # Add value labels with smart formatting
    for bars in [bars1_p99, bars2_p99]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                # Format large numbers with 'k' suffix
                if height >= 1000:
                    label = f'{height/1000:.1f}k'
                else:
                    label = f'{height:.0f}'
                ax3.text(bar.get_x() + bar.get_width()/2.0, height,
                        label, ha='center', va='bottom', fontsize=6, rotation=45)

    ax3.set_xlabel("Number of Clients", fontsize=11)
    ax3.set_ylabel("Latency (microseconds)", fontsize=11)
    ax3.set_title("p99 (Tail) Latency Comparison", fontsize=12, fontweight='bold')
    ax3.set_xticks(x)
    ax3.set_xticklabels(all_clients)
    ax3.legend(fontsize=9)
    ax3.grid(axis='y', linestyle='--', alpha=0.7)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"✓ Latency comparison saved as {output_file}")

def print_summary_statistics(baseline_df, uintrpoller_df, baseline_lat_df, uintrpoller_lat_df):
    """Print summary statistics for comparison."""
    print("\n" + "="*70)
    print("SUMMARY STATISTICS")
    print("="*70)

    # Throughput summary
    baseline_throughput = baseline_df.groupby("clients")["throughput"].mean() / 1e6
    uintrpoller_throughput = uintrpoller_df.groupby("clients")["throughput"].mean() / 1e6

    print("\nThroughput (Million req/sec):")
    print(f"{'Clients':<10} {'Baseline':<12} {'UINTR Poller':<12} {'Difference':<12} {'% Change':<10}")
    print("-" * 70)

    for clients in sorted(set(baseline_throughput.index) & set(uintrpoller_throughput.index)):
        base_val = baseline_throughput[clients]
        uintr_val = uintrpoller_throughput[clients]
        diff = uintr_val - base_val
        pct_change = (diff / base_val) * 100 if base_val > 0 else 0

        print(f"{clients:<10} {base_val:<12.2f} {uintr_val:<12.2f} {diff:<+12.2f} {pct_change:<+10.1f}%")

    # Latency summary (average latency)
    baseline_latency = baseline_lat_df.groupby("clients")["avg_latency_ns"].mean() / 1000
    uintrpoller_latency = uintrpoller_lat_df.groupby("clients")["avg_latency_ns"].mean() / 1000

    print("\nAverage Latency (microseconds):")
    print(f"{'Clients':<10} {'Baseline':<12} {'UINTR Poller':<12} {'Difference':<12} {'% Change':<10}")
    print("-" * 70)

    for clients in sorted(set(baseline_latency.index) & set(uintrpoller_latency.index)):
        base_val = baseline_latency[clients]
        uintr_val = uintrpoller_latency[clients]
        diff = uintr_val - base_val
        pct_change = (diff / base_val) * 100 if base_val > 0 else 0

        print(f"{clients:<10} {base_val:<12.2f} {uintr_val:<12.2f} {diff:<+12.2f} {pct_change:<+10.1f}%")

    print("="*70 + "\n")

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    baseline_dir = sys.argv[1]
    uintrpoller_dir = sys.argv[2]

    # Validate directories exist
    if not os.path.isdir(baseline_dir):
        print(f"Error: Baseline directory not found: {baseline_dir}")
        sys.exit(1)
    if not os.path.isdir(uintrpoller_dir):
        print(f"Error: UINTR poller directory not found: {uintrpoller_dir}")
        sys.exit(1)

    # Create timestamped comparison directory
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    comparison_dir = f"comparison_{timestamp}"
    os.makedirs(comparison_dir, exist_ok=True)

    print(f"Comparing results:")
    print(f"  Baseline:     {baseline_dir}")
    print(f"  UINTR Poller: {uintrpoller_dir}")
    print(f"  Output dir:   {comparison_dir}/")
    print()

    # Load data
    print("Loading throughput data...")
    baseline_throughput_df, uintrpoller_throughput_df = load_throughput_data(baseline_dir, uintrpoller_dir)

    print("Loading latency data...")
    baseline_latency_df, uintrpoller_latency_df = load_latency_data(baseline_dir, uintrpoller_dir)

    # Generate plots with paths inside comparison directory
    print("\nGenerating comparison plots...")
    throughput_plot = os.path.join(comparison_dir, "throughput_vs_clients_comparison.png")
    latency_plot = os.path.join(comparison_dir, "latency_vs_clients_comparison.png")

    plot_throughput_comparison(baseline_throughput_df, uintrpoller_throughput_df, throughput_plot)
    plot_latency_comparison(baseline_latency_df, uintrpoller_latency_df, latency_plot)

    # Print summary statistics
    print_summary_statistics(baseline_throughput_df, uintrpoller_throughput_df,
                            baseline_latency_df, uintrpoller_latency_df)

    print(f"\nComparison complete! Results saved to: {comparison_dir}/")

if __name__ == "__main__":
    main()
