import subprocess
import time
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(BASE_DIR, "build")

# مسیر فایل‌های اجرایی جدید
EXEC_STD = os.path.join(BUILD_DIR, "dds_standard")
EXEC_ADP = os.path.join(BUILD_DIR, "dds_adaptive")

CONFIG_PATH = os.path.join(BASE_DIR, "fastdds_config.xml")

# متغیرهای محیطی
env_vars = os.environ.copy()
if os.path.exists(CONFIG_PATH):
    env_vars["FASTRTPS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH
    env_vars["FASTDDS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH

def run_cmd(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def apply_netem(loss, delay, jitter=2):
    # پاک کردن قوانین قبلی شبکه
    run_cmd("sudo tc qdisc del dev lo root")
    run_cmd("sudo tc qdisc del dev eth0 root")

    if loss > 0 or delay > 0:
        if loss >= 100:
            cmd_lo = "sudo tc qdisc add dev lo root netem loss 100%"
            cmd_eth = "sudo tc qdisc add dev eth0 root netem loss 100%"
        else:
            cmd_lo = f"sudo tc qdisc add dev lo root netem delay {delay}ms {jitter}ms loss {loss}%"
            cmd_eth = f"sudo tc qdisc add dev eth0 root netem delay {delay}ms {jitter}ms loss {loss}%"

        run_cmd(cmd_lo)
        run_cmd(cmd_eth)

def run_scenario(mode):
    print(f"\n==========================================")
    print(f"    Running {mode.upper()} Scenario")
    print(f"==========================================")
    apply_netem(0, 0)

    exec_binary = EXEC_ADP if mode == "adaptive" else EXEC_STD
    sub_log = f"/tmp/dds_logs/{mode}_run.log"

    with open(sub_log, "w") as log_file:
        # ۱. اجرای Subscriber
        sub_proc = subprocess.Popen([exec_binary, "subscriber"], stdout=log_file, stderr=subprocess.STDOUT, env=env_vars)

        print("[+] Waiting 3s for DDS Discovery...")
        time.sleep(3)

        # ۲. اجرای Publisher
        pub_proc = subprocess.Popen([exec_binary, "publisher"], env=env_vars)

        # Stage 1: Optimal Condition
        print("[Stage 1] Optimal Condition (0ms delay, 0% loss)")
        apply_netem(loss=0, delay=0)
        time.sleep(6)

        # Stage 2: Mild DDIL
        print("[Stage 2] Mild DDIL (150ms delay, 10% loss)")
        apply_netem(loss=10, delay=150)
        time.sleep(6)

        # Stage 3: Moderate DDIL
        print("[Stage 3] Moderate DDIL (400ms delay, 30% loss)")
        apply_netem(loss=30, delay=400)
        time.sleep(6)

        # Stage 4: Severe DDIL / Blackout
        print("[Stage 4] Severe DDIL (800ms delay, 60% loss)")
        apply_netem(loss=60, delay=800)
        time.sleep(6)

        # Stage 5: Recovery
        print("[Stage 5] Recovery (0ms delay, 0% loss)")
        apply_netem(loss=0, delay=0)
        time.sleep(6)

        pub_proc.wait()
        sub_proc.terminate()
        apply_netem(0, 0)

os.makedirs("/tmp/dds_logs", exist_ok=True)

# پاکسازی پروسه‌های احتمالی قبلی
os.system("pkill -9 -f dds_standard > /dev/null 2>&1")
os.system("pkill -9 -f dds_adaptive > /dev/null 2>&1")

# اجرای سناریوها
run_scenario("standard")
time.sleep(3)
run_scenario("adaptive")

print("\n[✔] Simulation Finished Successfully!")
