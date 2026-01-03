import pandas as pd
import matplotlib.pyplot as plt

# Read the CSV with all runs
df = pd.read_csv("uintrpoller_cpu0_all_runs.log")

# Convert to float
for col in ["cpu_usr", "cpu_sys", "cpu_idle"]:
    df[col] = pd.to_numeric(df[col], errors='coerce')
df = df.dropna(subset=["cpu_usr", "cpu_sys", "cpu_idle"])

plt.figure(figsize=(12, 6))

# Plot lines
plt.plot(df.index, df["cpu_usr"], color="red", linestyle='-', label="%usr")
plt.plot(df.index, df["cpu_sys"], color="green", linestyle='-', label="%sys")
plt.plot(df.index, df["cpu_idle"], color="blue", linestyle='-', label="%idle")

plt.xlabel("Sample Index")
plt.ylabel("CPU 0 Utilization (%)")
plt.title("UINTR Poller: CPU 0 Utilization Over Time (All Runs)")
plt.ylim(0, 105)
plt.legend()
plt.grid(True, linestyle="--", alpha=0.6)
plt.tight_layout()
plt.savefig("uintrpoller_cpu0_utilization_all_runs.png", dpi=300)
print("Line chart saved as uintrpoller_cpu0_utilization_all_runs.png")
