import pandas as pd
import matplotlib.pyplot as plt

# Load CSV file
data = pd.read_csv("baseline_test_results.csv")

# Compute average throughput per number of clients
avg_throughput = data.groupby("clients")["throughput"].mean()

# Plot as a proper bar chart
plt.figure(figsize=(10,6))
bars = plt.bar(avg_throughput.index.astype(str), avg_throughput.values / 1e6, color='steelblue')  # in M req/sec

# Add value labels on top of each bar
for bar in bars:
    height = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2.0, height, f'{height:.2f}', ha='center', va='bottom')

plt.xlabel("Number of Clients")
plt.ylabel("Average Throughput (Million req/sec)")
plt.title("Baseline: Synchronizer Throughput vs Number of Clients")
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()

# Save figure as PNG
plt.savefig("throughput_vs_clients_bar.png", dpi=300)
print("Bar chart saved as throughput_vs_clients_bar.png")
