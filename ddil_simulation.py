import subprocess
import time
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(BASE_DIR, "build")

EXEC_STD = os.path.join(BUILD_DIR, "dds_standard")
EXEC_ADP = os.path.join(BUILD_DIR, "dds_adaptive")

CONFIG_PATH = os.path.join(BASE_DIR, "fastdds_config.xml")

env_vars = os.environ.copy()
if os.path.exists(CONFIG_PATH):
    env_vars["FASTRTPS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH
    env_vars["FASTDDS_DEFAULT_PROFILES_FILE"] = CONFIG_PATH

def run_cmd(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def apply_netem(loss, delay, jitter=2):
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
    print(f"    Running {mode.upper()} Scenario (6-Stage DDIL)")
    print(f"==========================================")
    apply_netem(0, 0)

    exec_binary = EXEC_ADP if mode == "adaptive" else EXEC_STD
    sub_log = f"/tmp/dds_logs/{mode}_run.log"

    with open(sub_log, "w") as log_file:
        sub_proc = subprocess.Popen([exec_binary, "subscriber"], stdout=log_file, stderr=subprocess.STDOUT, env=env_vars)

        print("[+] Waiting 3s for DDS Discovery...")
        time.sleep(3)

        pub_proc = subprocess.Popen([exec_binary, "publisher"], env=env_vars)

        # Stage 1: Optimal
        print("[Stage 1] Optimal Condition (0ms delay, 0% loss)")
        apply_netem(loss=0, delay=0)
        time.sleep(5)

        # Stage 2: Mild DDIL
        print("[Stage 2] Mild DDIL (100ms delay, 5% loss)")
        apply_netem(loss=5, delay=100)
        time.sleep(5)

        # Stage 3: Moderate DDIL
        print("[Stage 3] Moderate DDIL (250ms delay, 15% loss)")
        apply_netem(loss=15, delay=250)
        time.sleep(5)

        # Stage 4: High DDIL
        print("[Stage 4] High DDIL (450ms delay, 30% loss)")
        apply_netem(loss=30, delay=450)
        time.sleep(5)

        # Stage 5: Severe DDIL
        print("[Stage 5] Severe DDIL (700ms delay, 50% loss)")
        apply_netem(loss=50, delay=700)
        time.sleep(5)

        # Stage 6: Extreme / Blackout
        print("[Stage 6] Extreme Blackout (1200ms delay, 80% loss)")
        apply_netem(loss=80, delay=1200)
        time.sleep(5)

        pub_proc.wait()
        sub_proc.terminate()
        apply_netem(0, 0)

os.makedirs("/tmp/dds_logs", exist_ok=True)

os.system("pkill -9 -f dds_standard > /dev/null 2>&1")
os.system("pkill -9 -f dds_adaptive > /dev/null 2>&1")

run_scenario("standard")
time.sleep(3)
run_scenario("adaptive")

print("\n[✔] 6-Stage Autonomous Simulation Finished Successfully!")
