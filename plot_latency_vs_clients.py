import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Load latency CSV
df = pd.read_csv("latency_all_runs.csv")

# Group by clients and compute average latency across all clients and runs
avg_latency = df.groupby("clients")["avg_latency_ns"].mean() / 1000  # Convert to microseconds
p50_latency = df.groupby("clients")["p50_latency_ns"].mean() / 1000
p99_latency = df.groupby("clients")["p99_latency_ns"].mean() / 1000

# Plot
plt.figure(figsize=(10, 6))

x = avg_latency.index.astype(str)
x_pos = np.arange(len(x))

# Plot bars
bars_avg = plt.bar(x_pos - 0.2, avg_latency.values, width=0.2, label='Avg', color='steelblue', alpha=0.8)
bars_p50 = plt.bar(x_pos, p50_latency.values, width=0.2, label='p50', color='orange', alpha=0.8)
bars_p99 = plt.bar(x_pos + 0.2, p99_latency.values, width=0.2, label='p99', color='red', alpha=0.8)

# Add value labels on bars
for bars in [bars_avg, bars_p50, bars_p99]:
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2.0, height,
                f'{height:.1f}', ha='center', va='bottom', fontsize=8)

plt.xticks(x_pos, x)
plt.xlabel("Number of Clients")
plt.ylabel("Latency (microseconds)")
plt.title("Baseline: Per-Request Latency vs Number of Clients")
plt.legend()
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()

# Save figure
plt.savefig("latency_vs_clients.png", dpi=300)
print("Latency plot saved as latency_vs_clients.png")
