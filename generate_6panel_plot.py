import re
import matplotlib.pyplot as plt
import os

LOG_DIR = "/tmp/dds_logs"
STD_LOG = os.path.join(LOG_DIR, "standard_run.log")
ADP_LOG = os.path.join(LOG_DIR, "adaptive_run.log")

def parse_log(filepath):
    ids, latencies, stages = [], [], []
    if not os.path.exists(filepath):
        return ids, latencies, stages

    with open(filepath, 'r') as f:
        for line in f:
            # پشتیبانی هم از 'Total Latency:' و هم از 'Latency:'
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

print(f"[DEBUG] Standard samples parsed: {len(std_ids)}")
print(f"[DEBUG] Adaptive samples parsed: {len(adp_ids)}")

def calculate_metrics(ids):
    if not ids:
        return []
    total_expected = max(ids)
    rec_so_far = 0
    id_set = set(ids)
    loss_rates = []
    for i in range(1, total_expected + 1):
        if i in id_set:
            rec_so_far += 1
        loss_rates.append(((i - rec_so_far) / i) * 100)
    return loss_rates

std_loss = calculate_metrics(std_ids)
adp_loss = calculate_metrics(adp_ids)

# ساخت شکل اصلی با ۶ پنل
fig, axes = plt.subplots(2, 3, figsize=(18, 10))
fig.suptitle('Comprehensive Performance Benchmark: Standard vs Adaptive QoS (DDIL Engine)', fontsize=16, fontweight='bold')

# پنل ۱: Latency
ax1 = axes[0, 0]
if std_ids:
    ax1.plot(std_ids, std_lat, label='Standard QoS (Static)', color='red', alpha=0.7, linestyle='--')
if adp_ids:
    ax1.plot(adp_ids, adp_lat, label='Adaptive QoS', color='green', linewidth=1.5)
ax1.set_title('1. End-to-End Latency (ms)')
ax1.set_xlabel('Sample ID')
ax1.set_ylabel('Latency (ms)')
ax1.grid(True, linestyle=':', alpha=0.6)
ax1.legend()

# پنل ۲: Loss Rate
ax2 = axes[0, 1]
if std_loss:
    ax2.plot(range(1, len(std_loss)+1), std_loss, label='Standard QoS', color='red', linestyle='--')
if adp_loss:
    ax2.plot(range(1, len(adp_loss)+1), adp_loss, label='Adaptive QoS', color='green', linewidth=1.5)
ax2.set_title('2. Cumulative Packet Loss Rate (%)')
ax2.set_xlabel('Expected Sample ID')
ax2.set_ylabel('Loss Rate (%)')
ax2.grid(True, linestyle=':', alpha=0.6)
ax2.legend()

# پنل ۳: Throughput
ax3 = axes[0, 2]
if std_ids:
    ax3.hist(std_ids, bins=20, alpha=0.4, label='Standard Received', color='red')
if adp_ids:
    ax3.hist(adp_ids, bins=20, alpha=0.4, label='Adaptive Received', color='green')
ax3.set_title('3. Message Reception Density (Throughput)')
ax3.set_xlabel('Sample ID Range')
ax3.set_ylabel('Received Packets Count')
ax3.grid(True, linestyle=':', alpha=0.6)
ax3.legend()

# پنل ۴: Adaptive Stage
ax4 = axes[1, 0]
if std_ids:
    ax4.step(std_ids, [1]*len(std_ids), color='red', linestyle='--', label='Standard (Fixed S1)')
if adp_stages:
    ax4.step(adp_ids, adp_stages, color='blue', where='post', label='Adaptive Engine')
ax4.set_title('4. Active Adaptive Stage (1 to 4)')
ax4.set_xlabel('Sample ID')
ax4.set_ylabel('Stage Level')
ax4.set_yticks([1, 2, 3, 4])
ax4.set_yticklabels(['S1 (Opt)', 'S2 (Mild)', 'S3 (Mod)', 'S4 (Sev)'])
ax4.grid(True, linestyle=':', alpha=0.6)
ax4.legend()

# پنل ۵: History Depth
ax5 = axes[1, 1]
if std_ids:
    ax5.step(std_ids, [20]*len(std_ids), color='red', linestyle='--', label='Standard (Fixed 20)')
if adp_stages:
    history_map = {1:20, 2:10, 3:3, 4:1}
    ax5.step(adp_ids, [history_map[s] for s in adp_stages], color='orange', where='post', label='Adaptive Depth')
ax5.set_title('5. Dynamic History Qos Depth (Buffer Size)')
ax5.set_xlabel('Sample ID')
ax5.set_ylabel('History Depth')
ax5.grid(True, linestyle=':', alpha=0.6)
ax5.legend()

# پنل ۶: Reliability Mode
ax6 = axes[1, 2]
if std_ids:
    ax6.step(std_ids, [1]*len(std_ids), color='red', linestyle='--', label='Standard (Fixed Reliable)')
if adp_stages:
    rel_map = {1:1, 2:1, 3:0, 4:0}
    ax6.step(adp_ids, [rel_map[s] for s in adp_stages], color='purple', where='post', label='Adaptive Rel')
ax6.set_title('6. Dynamic Reliability QoS Mode')
ax6.set_xlabel('Sample ID')
ax6.set_ylabel('Mode')
ax6.set_yticks([0, 1])
ax6.set_yticklabels(['Best Effort', 'Reliable'])
ax6.grid(True, linestyle=':', alpha=0.6)
ax6.legend()

plt.tight_layout()
plt.subplots_adjust(top=0.92)
plt.savefig(os.path.join(LOG_DIR, 'dds_comprehensive_benchmark_6panels.png'), dpi=200)
plt.close()

print("[✔] 6-Panel Benchmark Plot generated successfully!")
