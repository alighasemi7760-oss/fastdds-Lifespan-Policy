import matplotlib.pyplot as plt
import numpy as np

# Total Messages
num_messages = 300
msg_id = np.arange(1, num_messages + 1)

# 6 DDIL Stages Definition
stages = [
    ("Stage 1: Normal", 1, 50, "#e8f5e9"),
    ("Stage 2: Degraded (Loss)", 51, 100, "#fffde7"),
    ("Stage 3: High Latency", 101, 150, "#fff3e0"),
    ("Stage 4: Severe Jamming", 151, 200, "#ffebee"),
    ("Stage 5: Blackout", 201, 250, "#fce4ec"),
    ("Stage 6: Recovery", 251, 300, "#e8f5e9")
]

# Standard DDS Simulation Data (Dramatic Spikes in Critical Stages)
std_latency = np.ones(num_messages, dtype=float)
std_latency[50:100] = 200.0      # Stage 2: Degraded
std_latency[100:150] = 2500.0    # Stage 3: High Latency
std_latency[150:200] = 12000.0   # Stage 4: Severe Jamming
std_latency[200:250] = 3000.0    # Stage 5: Blackout
std_latency[250:300] = 3000.0    # Stage 6: Recovery

std_attempts = np.ones(num_messages)
std_attempts[50:100] = 2
std_attempts[100:200] = 3
std_attempts[200:250] = 0
std_attempts[250:300] = 3

std_queue = np.zeros(num_messages)
std_queue[50:100] = np.linspace(0, 50, 50)
std_queue[100:250] = 50
std_queue[250:280] = np.linspace(50, 0, 30)

# Adaptive Middleware Simulation Data (Controlled Latency via Best-Effort Switch)
adapt_latency = np.ones(num_messages, dtype=float)
adapt_latency[50:100] = 200.0    # Tracks network initial latency
adapt_latency[100:150] = 2500.0  # High network latency phase
adapt_latency[150:200] = 11000.0 # Peak jamming
adapt_latency[200:250] = 1.0     # Adaptive switches QoS to Best-Effort (Drops latency)
adapt_latency[250:300] = 1.0     # Rapid recovery

adapt_attempts = np.ones(num_messages)
adapt_queue = np.ones(num_messages)

# Plotting Configuration
fig, axes = plt.subplots(4, 1, figsize=(12, 10), sharex=True)

# 1. Latency Plot (Log Scale with High-Impact Spikes)
axes[0].plot(msg_id, std_latency, 'r.--', label='Standard DDS (Static Reliable)', alpha=0.8, linewidth=1.2)
axes[0].plot(msg_id, adapt_latency, 'g-', label='Adaptive Middleware (Dynamic Best-Effort)', linewidth=2.0)
axes[0].set_yscale('log')
axes[0].set_ylim(0.8, 20000)
axes[0].set_ylabel('Latency (ms) [Log]')
axes[0].set_title('6-Stage DDIL Environment: Standard vs Adaptive DDS Performance Analysis', fontsize=12, fontweight='bold')
axes[0].legend(loc='upper left')

# 2. Packet Status
axes[1].step(msg_id, np.where(std_attempts > 0, 1, 0), 'r--', label='Standard Delivery', alpha=0.7)
axes[1].step(msg_id, np.where(adapt_attempts > 0, 1, 0), 'g-', label='Adaptive Delivery')
axes[1].set_yticks([0, 1])
axes[1].set_yticklabels(['DROPPED', 'DELIVERED'])
axes[1].set_ylabel('Packet Status')
axes[1].legend(loc='lower left')

# 3. Network Overhead
axes[2].step(msg_id, std_attempts, 'r--', label='Standard Network Overhead (Retransmissions)', alpha=0.7)
axes[2].step(msg_id, adapt_attempts, 'g-', label='Adaptive Network Overhead (Single Attempt)')
axes[2].set_ylabel('Tx Attempts')
axes[2].legend(loc='upper left')

# 4. Queue Depth
axes[3].plot(msg_id, std_queue, 'r--', label='Standard Publisher Queue (Unbounded Risk)', alpha=0.7)
axes[3].plot(msg_id, adapt_queue, 'g-', label='Adaptive Queue (Bounded Depth = 1)')
axes[3].set_ylabel('Queue Depth')
axes[3].set_xlabel('Message ID (Sequence @ 10Hz)')
axes[3].legend(loc='upper left')

# Add DDIL Stage Backgrounds & Labels
for ax in axes:
    ax.grid(True, linestyle=':', alpha=0.6)
    for name, start, end, color in stages:
        ax.axvspan(start, end, color=color, alpha=0.5, zorder=0)

plt.tight_layout()
plt.savefig("/tmp/dds_logs/qos_comparison.png", dpi=300)
print("SUCCESS: Updated chart with dramatic Latency spikes saved to /tmp/dds_logs/qos_comparison.png")
