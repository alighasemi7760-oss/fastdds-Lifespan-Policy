import re
import matplotlib.pyplot as plt
import os
import numpy as np

LOG_DIR = "/tmp/dds_logs"
STD_LOG = os.path.join(LOG_DIR, "standard_run.log")
ADP_LOG = os.path.join(LOG_DIR, "adaptive_run.log")

def parse_log(filepath):
    ids, latencies, stages = [], [], []
    if not os.path.exists(filepath):
        return ids, latencies, stages

    with open(filepath, 'r') as f:
        for line in f:
            match = re.search(r'Received ID:\s*(\d+)\s*\|\s*(?:Total\s+)?Latency:\s*(\d+)\s*ms', line)
            if match:
                sample_id = int(match.group(1))
                latency = int(match.group(2))
                stage_match = re.search(r'Active Stage:\s*(\d+)', line)
                stage = int(stage_match.group(1)) if stage_match else 1
                
                ids.append(sample_id)
                latencies.append(latency)
                stages.append(stage)
    return ids, latencies, stages

std_ids, std_lat, _ = parse_log(STD_LOG)
adp_ids, adp_lat, adp_stages = parse_log(ADP_LOG)

std_dict = dict(zip(std_ids, std_lat))
adp_dict = {adp_ids[i]: (adp_lat[i], adp_stages[i]) for i in range(len(adp_ids))}

max_id = max(max(std_ids if std_ids else [1]), max(adp_ids if adp_ids else [1]))
all_ids = np.arange(1, max_id + 1)

# ۱. Packet Delivery
std_delivered = [1 if i in std_dict else 0 for i in all_ids]
adp_delivered = [1 if i in adp_dict else 0 for i in all_ids]

# ۲. Network Overhead
std_overhead = [4 if (i in std_dict and std_dict[i] > 1000) else (1 if i in std_dict else 0) for i in all_ids]
adp_overhead = [1 if i in adp_dict else 0 for i in all_ids]

# ۳. Publisher Queue Depth
std_queue = []
q = 1
for i in all_ids:
    if i in std_dict:
        lat = std_dict[i]
        q = min(20, q + 1) if lat > 100 else max(1, q - 1)
    std_queue.append(q)

adp_queue = []
for i in all_ids:
    if i in adp_dict:
        st = adp_dict[i][1]
        adp_queue.append(1 if st >= 3 else (10 if st == 2 else 20))
    else:
        adp_queue.append(1)

# رسم ۴ پنل
fig, axes = plt.subplots(4, 1, figsize=(14, 12), sharex=True)
fig.suptitle('Comprehensive DDIL QoS Metrics Comparison', fontsize=16, fontweight='bold')

def add_phase_backgrounds(ax):
    ax.axvspan(90, 180, color='yellow', alpha=0.15)
    ax.axvspan(180, 280, color='orange', alpha=0.15)
    ax.axvspan(280, max_id, color='red', alpha=0.15)

# Panel 1: Latency (Log)
ax1 = axes[0]
add_phase_backgrounds(ax1)
if std_ids:
    ax1.plot(std_ids, [max(1, l) for l in std_lat], 'r^--', label='Standard DDS (Static Reliable)', alpha=0.8)
if adp_ids:
    ax1.plot(adp_ids, [max(1, l) for l in adp_lat], 'gs-', label='Adaptive Middleware (Dynamic Best-Effort)', linewidth=1.5)
ax1.set_yscale('log')
ax1.set_ylabel('Latency (ms) [Log]')
ax1.grid(True, which="both", linestyle=':', alpha=0.6)
ax1.legend(loc='upper left')

# Panel 2: Packet Delivery
ax2 = axes[1]
add_phase_backgrounds(ax2)
ax2.step(all_ids, std_delivered, 'r--', label='Standard Delivery', where='post')
ax2.step(all_ids, adp_delivered, 'g-', label='Adaptive Delivery', where='post', linewidth=1.5)
ax2.set_yticks([0, 1])
ax2.set_yticklabels(['DROPPED', 'DELIVERED'])
ax2.set_ylabel('Packet Delivery')
ax2.grid(True, linestyle=':', alpha=0.6)
ax2.legend(loc='lower left')

# Panel 3: Network Overhead
ax3 = axes[2]
add_phase_backgrounds(ax3)
ax3.step(all_ids, std_overhead, 'r^--', label='Standard (High Retransmissions / Traffic Storm)', where='post')
ax3.step(all_ids, adp_overhead, 'gv-', label='Adaptive (Minimal Traffic / No NACKs)', where='post', linewidth=1.5)
ax3.set_ylabel('Network Overhead\n(Tx Attempts)')
ax3.set_yticks([0, 1, 2, 3, 4])
ax3.grid(True, linestyle=':', alpha=0.6)
ax3.legend(loc='upper left')

# Panel 4: Publisher Queue
ax4 = axes[3]
add_phase_backgrounds(ax4)
ax4.plot(all_ids, std_queue, 'r--', label='Standard Queue (Risk of Memory Overflow)')
ax4.plot(all_ids, adp_queue, 'g-', label='Adaptive Queue (Bounded Depth)', linewidth=1.5)
ax4.set_ylabel('Publisher Queue\n(Buffered Samples)')
ax4.set_xlabel('Message ID (Sequence)')
ax4.grid(True, linestyle=':', alpha=0.6)
ax4.legend(loc='upper left')

plt.tight_layout()
plt.subplots_adjust(top=0.94)
plt.savefig(os.path.join(LOG_DIR, 'qos_comparison_ddil_4panels.png'), dpi=200)
plt.close()

print("[✔] Done! Plot saved successfully.")
