#!/bin/bash
set -e

cd ~/dds_qos_project

echo "[1/3] Running 6-Stage DDIL Simulation..."
python3 ddil_simulation.py

echo "[2/3] Generating 6-Panel Autonomous Plot..."
python3 generate_6panel_plot.py

echo "[3/3] Generating 4-Panel DDIL Phases Plot..."
python3 generate_ddil_phases_plot.py

echo -e "\n[✔] All simulations completed and plots successfully updated!"
