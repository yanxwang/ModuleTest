import pandas as pd
import matplotlib.pyplot as plt

# Load CSV
df = pd.read_csv("uintrpoller_throughput_all_runs.csv")

# Plot throughput per second
plt.figure(figsize=(10, 6))
for (clients, run), group in df.groupby(["clients", "run"]):
    plt.plot(group["time_sec"], group["throughput"], '-', label=f"{clients} clients, run {run}")

plt.xlabel("Time (s)")
plt.ylabel("Throughput (req/sec)")
plt.title("UINTR Poller: Per-Second Throughput Over Time")
plt.legend(fontsize=8)
plt.grid(True)
plt.tight_layout()
plt.ylim(bottom=0)
plt.savefig("uintrpoller_throughput_over_time.png", dpi=200)
print("Line chart saved as uintrpoller_throughput_over_time.png")
