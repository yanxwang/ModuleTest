#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read latency statistics
latency_df = pd.read_csv('latency_stats_all_runs.csv')

# Read throughput results
throughput_df = pd.read_csv('cxl_latency_results.csv')

# Create figure with multiple subplots
fig = plt.figure(figsize=(16, 12))

# ========== Plot 1: Throughput vs Clients ==========
ax1 = plt.subplot(2, 3, 1)
throughput_grouped = throughput_df.groupby('clients')['throughput'].agg(['mean', 'std'])
clients = throughput_grouped.index
mean_tput = throughput_grouped['mean'] / 1e6  # Convert to M req/sec
std_tput = throughput_grouped['std'] / 1e6

ax1.errorbar(clients, mean_tput, yerr=std_tput, marker='o', capsize=5, linewidth=2)
ax1.set_xlabel('Number of Clients', fontsize=12)
ax1.set_ylabel('Throughput (M req/sec)', fontsize=12)
ax1.set_title('Throughput vs Number of Clients', fontsize=14, fontweight='bold')
ax1.grid(True, alpha=0.3)

# ========== Plot 2: p50 Latency vs Clients ==========
ax2 = plt.subplot(2, 3, 2)
p50_grouped = latency_df.groupby('clients')['p50_ns'].agg(['mean', 'std'])
mean_p50 = p50_grouped['mean'] / 1000  # Convert to microseconds
std_p50 = p50_grouped['std'] / 1000

ax2.errorbar(clients, mean_p50, yerr=std_p50, marker='s', capsize=5, linewidth=2, color='green')
ax2.set_xlabel('Number of Clients', fontsize=12)
ax2.set_ylabel('p50 Latency (μs)', fontsize=12)
ax2.set_title('Median (p50) Latency vs Clients', fontsize=14, fontweight='bold')
ax2.grid(True, alpha=0.3)

# ========== Plot 3: p99 Latency vs Clients ==========
ax3 = plt.subplot(2, 3, 3)
p99_grouped = latency_df.groupby('clients')['p99_ns'].agg(['mean', 'std'])
mean_p99 = p99_grouped['mean'] / 1000  # Convert to microseconds
std_p99 = p99_grouped['std'] / 1000

ax3.errorbar(clients, mean_p99, yerr=std_p99, marker='^', capsize=5, linewidth=2, color='red')
ax3.set_xlabel('Number of Clients', fontsize=12)
ax3.set_ylabel('p99 Latency (μs)', fontsize=12)
ax3.set_title('Tail (p99) Latency vs Clients', fontsize=14, fontweight='bold')
ax3.grid(True, alpha=0.3)

# ========== Plot 4: Latency Distribution (Box Plot) ==========
ax4 = plt.subplot(2, 3, 4)

# Prepare data for box plot
box_data = []
box_labels = []
for client_count in sorted(latency_df['clients'].unique()):
    client_data = latency_df[latency_df['clients'] == client_count]
    # Combine p50, p90, p95, p99 for distribution visualization
    percentiles = []
    for _, row in client_data.iterrows():
        percentiles.extend([row['p50_ns'], row['p90_ns'], row['p95_ns'], row['p99_ns']])
    box_data.append([p/1000 for p in percentiles])  # Convert to μs
    box_labels.append(f"{client_count} clients")

bp = ax4.boxplot(box_data, tick_labels=box_labels, patch_artist=True)
for patch in bp['boxes']:
    patch.set_facecolor('lightblue')

ax4.set_ylabel('Latency (μs)', fontsize=12)
ax4.set_title('Latency Distribution by Client Count', fontsize=14, fontweight='bold')
ax4.grid(True, alpha=0.3, axis='y')

# ========== Plot 5: Percentile Comparison ==========
ax5 = plt.subplot(2, 3, 5)

# For a specific client count (or average), show all percentiles
# Use the minimum client count from the actual data
specific_clients = latency_df['clients'].min()
if specific_clients in latency_df['clients'].values:
    data = latency_df[latency_df['clients'] == specific_clients]
    percentiles = ['p50', 'p90', 'p95', 'p99']
    percentile_labels = ['p50', 'p90', 'p95', 'p99']

    means = [data[f'{p}_ns'].mean() / 1000 for p in percentiles]
    stds = [data[f'{p}_ns'].std() / 1000 for p in percentiles]

    x_pos = np.arange(len(percentile_labels))
    ax5.bar(x_pos, means, yerr=stds, capsize=5, alpha=0.7, color=['green', 'orange', 'red', 'darkred'])
    ax5.set_xticks(x_pos)
    ax5.set_xticklabels(percentile_labels)
    ax5.set_ylabel('Latency (μs)', fontsize=12)
    ax5.set_title(f'Latency Percentiles ({specific_clients} clients)', fontsize=14, fontweight='bold')
    ax5.grid(True, alpha=0.3, axis='y')

# ========== Plot 6: Per-Client Latency Breakdown ==========
ax6 = plt.subplot(2, 3, 6)

# Show p50 latency for each individual client (for one specific configuration)
# Use the minimum client count from the actual data
min_clients = latency_df['clients'].min()
specific_run = latency_df[(latency_df['clients'] == min_clients) & (latency_df['run'] == 1)]
if not specific_run.empty:
    client_ids = specific_run['client_id'].values
    p50_values = specific_run['p50_ns'].values / 1000  # μs
    p99_values = specific_run['p99_ns'].values / 1000  # μs

    x_pos = np.arange(len(client_ids))
    width = 0.35

    ax6.bar(x_pos - width/2, p50_values, width, label='p50', alpha=0.8, color='green')
    ax6.bar(x_pos + width/2, p99_values, width, label='p99', alpha=0.8, color='red')

    ax6.set_xticks(x_pos)
    ax6.set_xticklabels([f'Client {i}' for i in client_ids])
    ax6.set_ylabel('Latency (μs)', fontsize=12)
    ax6.set_title(f'Per-Client Latency ({min_clients} clients, run 1)', fontsize=14, fontweight='bold')
    ax6.legend()
    ax6.grid(True, alpha=0.3, axis='y')

plt.tight_layout()
plt.savefig('latency_analysis.png', dpi=150, bbox_inches='tight')
print("Saved latency analysis plot: latency_analysis.png")

# ========== Additional Plot: Latency-Throughput Tradeoff ==========
fig2, ax = plt.subplots(figsize=(10, 6))

# Merge throughput and latency data
merged = pd.merge(
    throughput_df.groupby('clients')['throughput'].mean().reset_index(),
    latency_df.groupby('clients')[['p50_ns', 'p99_ns']].mean().reset_index(),
    on='clients'
)

merged['throughput_M'] = merged['throughput'] / 1e6
merged['p50_us'] = merged['p50_ns'] / 1000
merged['p99_us'] = merged['p99_ns'] / 1000

# Plot throughput vs latency
ax.scatter(merged['throughput_M'], merged['p50_us'], s=100, label='p50 latency', marker='o')
ax.scatter(merged['throughput_M'], merged['p99_us'], s=100, label='p99 latency', marker='^', color='red')

# Annotate with client counts
for _, row in merged.iterrows():
    ax.annotate(f"{int(row['clients'])} clients",
                (row['throughput_M'], row['p50_us']),
                textcoords="offset points", xytext=(5,5), fontsize=9)

ax.set_xlabel('Throughput (M req/sec)', fontsize=12)
ax.set_ylabel('Latency (μs)', fontsize=12)
ax.set_title('Latency-Throughput Tradeoff', fontsize=14, fontweight='bold')
ax.legend()
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('latency_throughput_tradeoff.png', dpi=150, bbox_inches='tight')
print("Saved latency-throughput tradeoff plot: latency_throughput_tradeoff.png")

# ========== Additional Plot: Throughput vs Clients Bar Chart ==========
fig3, ax = plt.subplots(figsize=(10, 6))

# Compute average throughput per number of clients
avg_throughput = throughput_df.groupby("clients")["throughput"].mean()

# Plot as a bar chart
bars = ax.bar(avg_throughput.index.astype(str), avg_throughput.values / 1e6, color='steelblue')

# Add value labels on top of each bar
for bar in bars:
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2.0, height, f'{height:.2f}',
            ha='center', va='bottom', fontsize=11, fontweight='bold')

ax.set_xlabel("Number of Clients", fontsize=12)
ax.set_ylabel("Average Throughput (Million req/sec)", fontsize=12)
ax.set_title("Synchronizer Throughput vs Number of Clients", fontsize=14, fontweight='bold')
ax.grid(axis='y', linestyle='--', alpha=0.7)

plt.tight_layout()
plt.savefig('throughput_vs_clients_bar.png', dpi=300, bbox_inches='tight')
print("Saved throughput bar chart: throughput_vs_clients_bar.png")

plt.show()
