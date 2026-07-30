import os
import pandas as pd
import matplotlib.pyplot as plt

# پیدا کردن مسیر صحیح فایل‌های لاگ
possible_paths = [
    '/tmp/dds_logs/',
    '/root/dds_qos_project/build/',
    '/root/dds_qos_project/',
    './'
]

log_dir = None
for p in possible_paths:
    if os.path.exists(os.path.join(p, 'adaptive_metrics.csv')):
        log_dir = p
        break

if not log_dir:
    print("[-] Error: 'adaptive_metrics.csv' not found in any standard path!")
    exit(1)

print(f"[+] Found log files in: {log_dir}")

# ۱. بارگذاری داده‌های لاگ
adaptive_df = pd.read_csv(os.path.join(log_dir, 'adaptive_metrics.csv'))
standard_df = pd.read_csv(os.path.join(log_dir, 'standard_metrics.csv'))

fig, axes = plt.subplots(4, 1, figsize=(12, 10), sharex=True)

# تعریف رنگ برای هر استیج
stage_colors = {
    1: '#e8f5e9', # Green (Optimal)
    2: '#fffde7', # Yellow (Mild)
    3: '#fff3e0', # Orange (Moderate)
    4: '#ffebee', # Light Red (High)
    5: '#f8bbd0', # Red (Severe)
    6: '#e1bee7'  # Purple (Extreme)
}

# ۲. رسم پس‌زمینه پویا منطبق بر Active Stage واقعی
msg_ids = adaptive_df['Message_ID'].values
stages = adaptive_df['Active_Stage'].values

for ax in axes:
    for i in range(len(stages) - 1):
        curr_stage = stages[i]
        color = stage_colors.get(curr_stage, '#ffffff')
        ax.axvspan(msg_ids[i], msg_ids[i+1], color=color, alpha=0.6, linewidth=0)

# ۳. رسم داده‌های Panel 1: Latency
axes[0].plot(standard_df['Message_ID'], standard_df['Latency_ms'], 'r--^', label='Standard DDS (Static Reliable)', markevery=10)
axes[0].plot(adaptive_df['Message_ID'], adaptive_df['Latency_ms'], 'g-s', label='Adaptive Middleware (Autonomous Best-Effort)', markevery=10)
axes[0].set_yscale('log')
axes[0].set_ylabel('Latency (ms) [Log]')
axes[0].legend(loc='upper left')

# ۴. رسم داده‌های Panel 2: Packet Delivery
axes[1].plot(standard_df['Message_ID'], standard_df['Delivered'], 'r--', label='Standard Delivery')
axes[1].plot(adaptive_df['Message_ID'], adaptive_df['Delivered'], 'g-', label='Adaptive Delivery')
axes[1].set_ylabel('Packet Delivery')
axes[1].legend(loc='lower left')

# ۵. رسم داده‌های Panel 3: Network Overhead
axes[2].plot(standard_df['Message_ID'], standard_df['Retransmissions'], 'r--^', label='Standard (High Retransmissions / Traffic Storm)')
axes[2].plot(adaptive_df['Message_ID'], adaptive_df['Retransmissions'], 'g-v', label='Adaptive (Minimal Traffic / No NACKs)')
axes[2].set_ylabel('Network Overhead\n(Tx Attempts)')
axes[2].legend(loc='upper left')

# ۶. رسم داده‌های Panel 4: Publisher Queue
axes[3].plot(standard_df['Message_ID'], standard_df['Queue_Depth'], 'r--', label='Standard Queue (Risk of Memory Overflow)')
axes[3].plot(adaptive_df['Message_ID'], adaptive_df['Queue_Depth'], 'g-', label='Adaptive Queue (Bounded Depth: 20->10->5->3->2->1)')
axes[3].set_ylabel('Publisher Queue\n(Buffered Samples)')
axes[3].set_xlabel('Message ID (Sequence)')
axes[3].legend(loc='lower left')

# تنظیمات کلی
plt.suptitle('Autonomous 6-Stage DDIL QoS Metrics Comparison', fontsize=14, fontweight='bold')
for ax in axes:
    ax.grid(True, linestyle=':', alpha=0.6)

plt.tight_layout()

# ذخیره خروجی در تمام پوشه‌های لازم جهت دسترسی وب‌سرور
output_filename = 'qos_comparison_ddil_4panels.png'
plt.savefig(os.path.join(log_dir, output_filename), dpi=300)
plt.savefig(f'/root/dds_qos_project/{output_filename}', dpi=300)

print(f"[+] Plot generated successfully and saved to /root/dds_qos_project/{output_filename}")
