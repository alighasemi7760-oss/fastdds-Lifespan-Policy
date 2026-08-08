"""
generate_thesis_plots.py

لاگ‌های تولیدشده توسط ddil_simulation.py را می‌خواند و نمودارهای استاندارد
مقایسه‌ای Standard در برابر Adaptive را برای فصل نتایج تز تولید می‌کند.

پیش‌نیاز:
    pip install matplotlib numpy --break-system-packages

اجرا (روی همون سروری که لاگ‌ها آنجا تولید شده‌اند، یا بعد از دانلود پوشه‌ی
/tmp/dds_logs به سیستم خودت):
    python3 generate_thesis_plots.py

خروجی در پوشه‌ی ./thesis_plots/ ذخیره می‌شود.

نکته‌ی مهم: خطوط لاگ زمان دقیق (wall-clock) ندارند، فقط ترتیب رخدادند.
محور x نمودارهای «روند زمانی» بر اساس شماره‌ی پکت یا شماره‌ی رخداد است،
نه ثانیه‌ی واقعی. اگر بعداً خواستی زمان واقعی هم داشته باشی، باید به خطوط
std::cout در فایل‌های cpp یک timestamp اضافه کنی.
"""

import os
import re
import csv
import statistics as stats

import matplotlib
matplotlib.use("Agg")  # بدون نیاز به نمایشگر گرافیکی (برای اجرا روی VPS)
import matplotlib.pyplot as plt

LOG_DIR = "/tmp/dds_logs"
OUT_DIR = "thesis_plots"
NUM_RUNS_TO_PLOT = 1  # کدام run برای نمودارهای «روند» (line plots) استفاده شود


# -------------------------------------------------------------------
# Parsers
# -------------------------------------------------------------------

def parse_standard_log(path):
    """[Subscriber-Standard] Received ID: N | Total Latency: L ms"""
    pattern = re.compile(r"Received ID:\s*(\d+)\s*\|\s*Total Latency:\s*(\d+)\s*ms")
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path) as f:
        for line in f:
            m = pattern.search(line)
            if m:
                rows.append((int(m.group(1)), int(m.group(2))))
    return rows


def parse_adaptive_log(path):
    """[Subscriber-Adaptive] Received ID: N | Latency: Lms | Jitter: Jms | LossRate: X% | Throughput: Tkbps"""
    pattern = re.compile(
        r"Received ID:\s*(\d+)\s*\|\s*Latency:\s*(\d+)ms\s*\|\s*Jitter:\s*(\d+)ms\s*\|\s*"
        r"LossRate:\s*(\d+)%\s*\|\s*Throughput:\s*([\d.]+)kbps"
    )
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path) as f:
        for line in f:
            m = pattern.search(line)
            if m:
                rows.append((
                    int(m.group(1)), int(m.group(2)), int(m.group(3)),
                    int(m.group(4)), float(m.group(5))
                ))
    return rows


def parse_publisher_log(path):
    """
    خطوط:
    [Autonomous QoS Engine] Dynamic Transition -> Stage S | Network Stress Index (NSI): N/100 (...)
    [Autonomous QoS Engine] Stage A -> B | New Lifespan: Lms | New Deadline: Dms (NSI: N/100)
    """
    transition_pat = re.compile(
        r"Stage (\d+) -> (\d+) \| New Lifespan:\s*(\d+)ms\s*\|\s*New Deadline:\s*(\d+)ms\s*\(NSI:\s*(\d+)/100\)"
    )
    nsi_pat = re.compile(
        r"Dynamic Transition -> Stage (\d+) \| Network Stress Index \(NSI\):\s*(\d+)/100"
    )
    transitions = []  # (from_stage, to_stage, lifespan_ms, deadline_ms, nsi)
    nsi_series = []    # (stage, nsi) — یک نمونه برای هر خط Dynamic Transition
    if not os.path.exists(path):
        return transitions, nsi_series
    with open(path) as f:
        for line in f:
            m1 = transition_pat.search(line)
            if m1:
                transitions.append((
                    int(m1.group(1)), int(m1.group(2)), int(m1.group(3)),
                    int(m1.group(4)), int(m1.group(5))
                ))
                continue
            m2 = nsi_pat.search(line)
            if m2:
                nsi_series.append((int(m2.group(1)), int(m2.group(2))))
    return transitions, nsi_series


def parse_summary_csv(path):
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append({
                "mode": r["mode"],
                "run": int(r["run"]),
                "received": int(r["received"]),
                "expected": int(r["expected"]),
                "completed": r["completed"] == "True",
                "drain_wait_seconds": float(r["drain_wait_seconds"]),
            })
    return rows


# -------------------------------------------------------------------
# Plots
# -------------------------------------------------------------------

def plot_delivery_ratio(summary_rows, out_dir):
    if not summary_rows:
        print("[skip] summary.csv یافت نشد یا خالی است.")
        return

    by_mode = {"standard": [], "adaptive": []}
    for r in summary_rows:
        if r["mode"] in by_mode:
            by_mode[r["mode"]].append(100.0 * r["received"] / r["expected"])

    modes = [m for m in ("standard", "adaptive") if by_mode[m]]
    means = [stats.mean(by_mode[m]) for m in modes]
    stds = [stats.pstdev(by_mode[m]) if len(by_mode[m]) > 1 else 0.0 for m in modes]

    fig, ax = plt.subplots(figsize=(5, 4))
    bars = ax.bar(modes, means, yerr=stds, capsize=8, color=["#4C72B0", "#DD8452"])
    ax.set_ylabel("Delivery Ratio (%)")
    ax.set_title(f"Packet Delivery Ratio — mean ± std over {len(by_mode.get('standard', by_mode['adaptive']))} runs")
    ax.set_ylim(0, 100)
    for bar, mean in zip(bars, means):
        ax.text(bar.get_x() + bar.get_width() / 2, mean + 2, f"{mean:.1f}%",
                 ha="center", va="bottom")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "delivery_ratio.png"), dpi=150)
    plt.close(fig)
    print("[ok] delivery_ratio.png")


def plot_latency_comparison(run_idx, out_dir):
    std_path = os.path.join(LOG_DIR, f"standard_run{run_idx}.log")
    adp_path = os.path.join(LOG_DIR, f"adaptive_run{run_idx}.log")
    std_rows = parse_standard_log(std_path)
    adp_rows = parse_adaptive_log(adp_path)

    if not std_rows and not adp_rows:
        print(f"[skip] لاگ‌های run {run_idx} یافت نشدند.")
        return

    fig, ax = plt.subplots(figsize=(9, 5))
    if std_rows:
        ids, lat = zip(*std_rows)
        ax.plot(ids, lat, label="Standard (Fixed QoS)", color="#DD8452", linewidth=1)
    if adp_rows:
        ids = [r[0] for r in adp_rows]
        lat = [r[1] for r in adp_rows]
        ax.plot(ids, lat, label="Adaptive (Lifespan+Deadline)", color="#4C72B0", linewidth=1)

    ax.set_xlabel("Packet ID")
    ax.set_ylabel("Latency (ms)")
    ax.set_yscale("log")
    ax.set_title(f"Latency per Packet — Standard vs Adaptive (run {run_idx})")
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, f"latency_comparison_run{run_idx}.png"), dpi=150)
    plt.close(fig)
    print(f"[ok] latency_comparison_run{run_idx}.png")


def plot_adaptive_qos_timeline(run_idx, out_dir):
    pub_path = os.path.join(LOG_DIR, f"adaptive_publisher_run{run_idx}.log")
    transitions, _ = parse_publisher_log(pub_path)
    if not transitions:
        print(f"[skip] هیچ Stage transition ای در publisher log run {run_idx} یافت نشد.")
        return

    events = list(range(1, len(transitions) + 1))
    lifespans = [t[2] for t in transitions]
    deadlines = [t[3] for t in transitions]

    fig, ax1 = plt.subplots(figsize=(9, 5))
    ax1.step(events, lifespans, where="post", color="#4C72B0", label="Lifespan (ms)")
    ax1.set_xlabel("Stage Transition #")
    ax1.set_ylabel("Lifespan (ms)", color="#4C72B0")
    ax1.tick_params(axis="y", labelcolor="#4C72B0")

    ax2 = ax1.twinx()
    ax2.step(events, deadlines, where="post", color="#DD8452", label="Deadline (ms)")
    ax2.set_ylabel("Deadline (ms)", color="#DD8452")
    ax2.tick_params(axis="y", labelcolor="#DD8452")

    ax1.set_title(f"Adaptive QoS over Stage Transitions (run {run_idx})\n"
                   f"Lifespan tightens, Deadline relaxes as network worsens")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, f"adaptive_qos_timeline_run{run_idx}.png"), dpi=150)
    plt.close(fig)
    print(f"[ok] adaptive_qos_timeline_run{run_idx}.png")


def plot_nsi_and_stage(run_idx, out_dir):
    pub_path = os.path.join(LOG_DIR, f"adaptive_publisher_run{run_idx}.log")
    _, nsi_series = parse_publisher_log(pub_path)
    if not nsi_series:
        print(f"[skip] هیچ NSI sample ای در publisher log run {run_idx} یافت نشد.")
        return

    events = list(range(1, len(nsi_series) + 1))
    stages = [s for s, _ in nsi_series]
    nsis = [n for _, n in nsi_series]

    fig, ax1 = plt.subplots(figsize=(9, 5))
    ax1.step(events, nsis, where="post", color="#C44E52", label="NSI")
    ax1.set_xlabel("Sample #")
    ax1.set_ylabel("Network Stress Index (0-100)", color="#C44E52")
    ax1.tick_params(axis="y", labelcolor="#C44E52")
    ax1.set_ylim(0, 100)

    ax2 = ax1.twinx()
    ax2.step(events, stages, where="post", color="#55A868", label="Stage")
    ax2.set_ylabel("DDIL Stage (1-6)", color="#55A868")
    ax2.tick_params(axis="y", labelcolor="#55A868")
    ax2.set_ylim(0.5, 6.5)

    ax1.set_title(f"NSI and DDIL Stage over Time (run {run_idx})")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, f"nsi_and_stage_run{run_idx}.png"), dpi=150)
    plt.close(fig)
    print(f"[ok] nsi_and_stage_run{run_idx}.png")


def plot_throughput_and_loss(run_idx, out_dir):
    adp_path = os.path.join(LOG_DIR, f"adaptive_run{run_idx}.log")
    adp_rows = parse_adaptive_log(adp_path)
    if not adp_rows:
        print(f"[skip] لاگ subscriber adaptive run {run_idx} یافت نشد.")
        return

    ids = [r[0] for r in adp_rows]
    loss = [r[3] for r in adp_rows]
    throughput = [r[4] for r in adp_rows]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9, 7), sharex=True)
    ax1.plot(ids, throughput, color="#4C72B0")
    ax1.set_ylabel("Throughput (kbps)")
    ax1.set_title(f"Observed Throughput and Loss Rate — Adaptive (run {run_idx})")
    ax1.grid(True, alpha=0.3)

    ax2.plot(ids, loss, color="#C44E52")
    ax2.set_ylabel("Loss Rate (%)")
    ax2.set_xlabel("Packet ID")
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, f"throughput_loss_run{run_idx}.png"), dpi=150)
    plt.close(fig)
    print(f"[ok] throughput_loss_run{run_idx}.png")


def plot_drain_wait(summary_rows, out_dir):
    if not summary_rows:
        return
    by_mode = {"standard": [], "adaptive": []}
    for r in summary_rows:
        if r["mode"] in by_mode:
            by_mode[r["mode"]].append(r["drain_wait_seconds"])

    modes = [m for m in ("standard", "adaptive") if by_mode[m]]
    means = [stats.mean(by_mode[m]) for m in modes]
    stds = [stats.pstdev(by_mode[m]) if len(by_mode[m]) > 1 else 0.0 for m in modes]

    fig, ax = plt.subplots(figsize=(5, 4))
    ax.bar(modes, means, yerr=stds, capsize=8, color=["#4C72B0", "#DD8452"])
    ax.set_ylabel("Drain Wait (seconds)")
    ax.set_title(f"Time to Reach Delivery Completion (capped at 30s)\nmean ± std")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "drain_wait.png"), dpi=150)
    plt.close(fig)
    print("[ok] drain_wait.png")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    summary_rows = parse_summary_csv(os.path.join(LOG_DIR, "summary.csv"))

    plot_delivery_ratio(summary_rows, OUT_DIR)
    plot_drain_wait(summary_rows, OUT_DIR)
    plot_latency_comparison(NUM_RUNS_TO_PLOT, OUT_DIR)
    plot_adaptive_qos_timeline(NUM_RUNS_TO_PLOT, OUT_DIR)
    plot_nsi_and_stage(NUM_RUNS_TO_PLOT, OUT_DIR)
    plot_throughput_and_loss(NUM_RUNS_TO_PLOT, OUT_DIR)

    print(f"\nAll available plots saved to ./{OUT_DIR}/")


if __name__ == "__main__":
    main()
