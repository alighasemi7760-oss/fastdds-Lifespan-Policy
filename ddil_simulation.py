import subprocess
import time
import os
import csv

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(BASE_DIR, "build")
EXEC_STD = os.path.join(BUILD_DIR, "dds_standard")
EXEC_ADP = os.path.join(BUILD_DIR, "dds_adaptive")
CONFIG_PATH = os.path.join(BASE_DIR, "fastdds_config.xml")

LOG_DIR = "/tmp/dds_logs"
SUMMARY_CSV = os.path.join(LOG_DIR, "summary.csv")

TOTAL_PACKETS = 300        # باید با تعداد نمونه‌های تولیدشده در فایل‌های cpp یکسان باشد
NUM_RUNS = 3                # تعداد تکرار هر سناریو (برای گزارش میانگین/انحراف‌معیار در تز افزایش بده، مثلاً 5 یا 10)
MAX_DRAIN_WAIT = 30         # حداکثر ثانیه‌ای که برای تکمیل تحویل صبر می‌کنیم (به‌جای sleep ثابت)

env_vars = os.environ.copy()
if os.path.exists(CONFIG_PATH):
    env_vars["FASTRTPS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH
    env_vars["FASTDDS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH

# -----------------------------------------------------------------------
# جدول مراحل DDIL: هر مرحله دارای delay/loss/jitter/rate/duration مستقل است.
# jitter دیگر مقدار ثابت نیست و متناسب با شدت هر مرحله افزایش می‌یابد.
# rate بُعد "Limited" (محدودیت پهنای‌باند) را که قبلاً اصلاً شبیه‌سازی نمی‌شد اضافه می‌کند.
# دو مرحله‌ی آخر (Blackout Probe و Recovery) قبلاً در اسکریپت وجود نداشتند:
#   - Blackout Probe: افت تقریباً کامل (99%) برای تست رفتار سیستم وقتی کانال Feedback
#     عملاً از کار می‌افتد (محدودیت شناخته‌شده‌ای که باید در تز مستند شود).
#   - Recovery: بازگشت به شرایط سالم، برای دیدن اینکه NetworkMonitor چقدر سریع
#     به Stage 1 برمی‌گردد (تست جهت معکوس Hysteresis).
# -----------------------------------------------------------------------
STAGES = [
    {"label": "Stage 1: Optimal Condition",       "delay": 0,    "loss": 0,  "jitter": 0,   "rate": None,       "duration": 5},
    {"label": "Stage 2: Mild DDIL",                "delay": 100,  "loss": 5,  "jitter": 15,  "rate": "10mbit",   "duration": 5},
    {"label": "Stage 3: Moderate DDIL",            "delay": 250,  "loss": 15, "jitter": 35,  "rate": "4mbit",    "duration": 5},
    {"label": "Stage 4: High DDIL",                "delay": 450,  "loss": 30, "jitter": 65,  "rate": "1mbit",    "duration": 5},
    {"label": "Stage 5: Severe DDIL",              "delay": 700,  "loss": 50, "jitter": 100, "rate": "512kbit",  "duration": 5},
    {"label": "Stage 6: Extreme Blackout",         "delay": 1200, "loss": 85, "jitter": 180, "rate": "128kbit",  "duration": 5},
    {"label": "Blackout Probe (near-total loss)",  "delay": 1500, "loss": 99, "jitter": 200, "rate": "64kbit",   "duration": 8},
    {"label": "Recovery (back to Optimal)",        "delay": 0,    "loss": 0,  "jitter": 0,   "rate": None,       "duration": 10},
]


def run_cmd(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def apply_netem(loss, delay, jitter=0, rate=None):
    run_cmd("sudo tc qdisc del dev lo root")
    run_cmd("sudo tc qdisc del dev eth0 root")

    if loss <= 0 and delay <= 0 and not rate:
        return  # شبکه سالم؛ هیچ qdisc ای اعمال نمی‌شود

    if loss >= 100:
        netem_args = "loss 100%"
    else:
        parts = []
        if delay > 0:
            parts.append(f"delay {delay}ms {jitter}ms")
        if loss > 0:
            parts.append(f"loss {loss}%")
        if rate:
            parts.append(f"rate {rate}")
        netem_args = " ".join(parts)

    for dev in ("lo", "eth0"):
        run_cmd(f"sudo tc qdisc add dev {dev} root netem {netem_args}")


def last_received_id(log_path):
    if not os.path.exists(log_path):
        return 0
    last_id = 0
    with open(log_path, "r") as f:
        for line in f:
            if "Received ID:" in line:
                try:
                    last_id = int(line.split("Received ID:")[1].split("|")[0].strip())
                except ValueError:
                    pass
    return last_id


def count_received(log_path):
    if not os.path.exists(log_path):
        return 0
    with open(log_path, "r") as f:
        return sum(1 for line in f if "Received ID:" in line)


def wait_for_completion(sub_log_path, expected_id=TOTAL_PACKETS, max_wait=MAX_DRAIN_WAIT, poll_interval=1):
    """به‌جای sleep ثابت، هر ثانیه چک می‌کند آیا آخرین پکت رسیده یا نه؛
    حداکثر تا max_wait ثانیه صبر می‌کند، نه بیشتر."""
    waited = 0
    while waited < max_wait:
        if last_received_id(sub_log_path) >= expected_id:
            return True, waited
        time.sleep(poll_interval)
        waited += poll_interval
    return last_received_id(sub_log_path) >= expected_id, waited


def append_summary(mode, run_idx, received, expected, completed, waited_s):
    write_header = not os.path.exists(SUMMARY_CSV)
    with open(SUMMARY_CSV, "a", newline="") as f:
        writer = csv.writer(f)
        if write_header:
            writer.writerow(["mode", "run", "received", "expected", "completed", "drain_wait_seconds"])
        writer.writerow([mode, run_idx, received, expected, completed, waited_s])


def run_scenario(mode, run_idx):
    print(f"\n==========================================")
    print(f"    Run {run_idx}/{NUM_RUNS} — {mode.upper()} Scenario (DDIL + Blackout Probe + Recovery)")
    print(f"==========================================")

    exec_binary = EXEC_ADP if mode == "adaptive" else EXEC_STD

    # اطمینان از اینکه پروسه‌ی باقی‌مانده از اجرای قبلی وجود ندارد
    run_cmd(f"pkill -9 -f {os.path.basename(exec_binary)}")
    time.sleep(1)

    apply_netem(0, 0)

    sub_log = os.path.join(LOG_DIR, f"{mode}_run{run_idx}.log")
    pub_log = os.path.join(LOG_DIR, f"{mode}_publisher_run{run_idx}.log")

    with open(sub_log, "w") as sub_log_file, open(pub_log, "w") as pub_log_file:
        sub_proc = subprocess.Popen(
            [exec_binary, "subscriber"],
            stdout=sub_log_file, stderr=subprocess.STDOUT, env=env_vars
        )
        print("[+] Waiting 3s for DDS Discovery...")
        time.sleep(3)

        pub_proc = subprocess.Popen(
            [exec_binary, "publisher"],
            stdout=pub_log_file, stderr=subprocess.STDOUT, env=env_vars
        )

        for stage in STAGES:
            print(f"[{stage['label']}] delay={stage['delay']}ms jitter={stage['jitter']}ms "
                  f"loss={stage['loss']}% rate={stage['rate']}")
            apply_netem(loss=stage["loss"], delay=stage["delay"], jitter=stage["jitter"], rate=stage["rate"])
            time.sleep(stage["duration"])

        pub_proc.wait()
        apply_netem(0, 0)

        print(f"[+] Waiting for delivery to complete (max {MAX_DRAIN_WAIT}s)...")
        completed, waited = wait_for_completion(sub_log)
        print(f"[+] Drain wait finished after {waited}s | Fully completed: {completed}")

        sub_proc.terminate()

    received = count_received(sub_log)
    append_summary(mode, run_idx, received, TOTAL_PACKETS, completed, waited)
    print(f"[+] {mode.upper()} run {run_idx}: {received}/{TOTAL_PACKETS} packets received "
          f"(completed={completed})")


os.makedirs(LOG_DIR, exist_ok=True)
if os.path.exists(SUMMARY_CSV):
    os.remove(SUMMARY_CSV)

os.system("pkill -9 -f dds_standard > /dev/null 2>&1")
os.system("pkill -9 -f dds_adaptive > /dev/null 2>&1")

for run_idx in range(1, NUM_RUNS + 1):
    print(f"\n#################### RUN {run_idx}/{NUM_RUNS} ####################")
    run_scenario("standard", run_idx)
    time.sleep(3)
    run_scenario("adaptive", run_idx)
    time.sleep(3)

print(f"\n[✔] All {NUM_RUNS} repetitions finished for both scenarios.")
print(f"[✔] Per-run summary written to: {SUMMARY_CSV}")
