import subprocess
import time
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXEC_PATH = os.path.join(BASE_DIR, "build", "DataSample")

def run_cmd(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def apply_netem(loss, delay, jitter=2):
    run_cmd("sudo tc qdisc del dev lo root")
    if loss > 0 or delay > 0:
        if loss >= 100:
            cmd = "sudo tc qdisc add dev lo root netem loss 100%"
        else:
            cmd = f"sudo tc qdisc add dev lo root netem delay {delay}ms {jitter}ms loss {loss}%"
        run_cmd(cmd)

def run_scenario(mode):
    print(f"--- Running {mode.upper()} Scenario ---")
    apply_netem(0, 0)
    
    sub_log = f"/tmp/dds_logs/subscriber_{mode}.log"
    sub_proc = subprocess.Popen([EXEC_PATH, "subscriber", mode], stdout=open(sub_log, "w"))
    time.sleep(2) # فرصت برای Discovery اولیه
    pub_proc = subprocess.Popen([EXEC_PATH, "publisher", mode])

    # Stage 1: Optimal (5s -> Msg 1-50)
    time.sleep(5)
    
    # Stage 2: Mild Jitter/Loss (5s -> Msg 51-100)
    apply_netem(loss=10, delay=15)
    time.sleep(5)
    
    # Stage 3: Moderate DDIL (5s -> Msg 101-150)
    apply_netem(loss=30, delay=35)
    time.sleep(5)
    
    # Stage 4: Severe Loss (5s -> Msg 151-200)
    apply_netem(loss=60, delay=60)
    time.sleep(5)
    
    # Stage 5: Blackout 100% Cut (5s -> Msg 201-250)
    apply_netem(loss=100, delay=0)
    time.sleep(5)
    
    # Stage 6: Recovery (5s -> Msg 251-300)
    apply_netem(loss=0, delay=0)
    time.sleep(6)

    pub_proc.wait()
    sub_proc.terminate()
    apply_netem(0, 0)

os.makedirs("/tmp/dds_logs", exist_ok=True)

run_scenario("standard")
time.sleep(2)
run_scenario("adaptive")
print("Simulation Finished!")
