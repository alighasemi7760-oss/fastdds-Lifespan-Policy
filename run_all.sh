#!/bin/bash
set -e

CD_DIR="/root/dds_qos_project"
WEB_DIR="/tmp/dds_logs"
NEW_PLOTS_DIR="/root/dds_qos_project/generated_plots"

cd "$CD_DIR"

echo "=================================================="
echo "[1/4] Running DDIL Simulation & Data Collection..."
echo "=================================================="
python3 ddil_simulation.py

echo "=================================================="
echo "[2/4] Generating Core & Extended Paper Plots..."
echo "=================================================="
python3 generate_6panel_plot.py || true
python3 generate_ddil_phases_plot.py || true

if [ -f "generate_all_paper_plots.py" ]; then
    python3 generate_all_paper_plots.py
else
    python3 generate_paper_plots.py || true
    python3 generate_extended_plots.py || true
fi

echo "=================================================="
echo "[3/4] Archiving New Plots & Updating Web Server..."
echo "=================================================="
# ۱. ایجاد دایرکتوری اختصاصی برای پلات‌های جدید و پاک‌سازی نمودارهای قبلی آن
mkdir -p "$NEW_PLOTS_DIR"
rm -f "$NEW_PLOTS_DIR"/*.png

# ۲. کپی نمودارهای تازه تولید شده به دایرکتوری جدید
cp plot*.png "$NEW_PLOTS_DIR/" 2>/dev/null || true
cp *.png "$NEW_PLOTS_DIR/" 2>/dev/null || true

# ۳. پاک‌سازی و جایگزینی نمودارها روی وب‌سرور
mkdir -p "$WEB_DIR"
rm -f "$WEB_DIR"/*.png

# انتقال لاگ‌های جدید
[ -f "sub.log" ] && cp sub.log "$WEB_DIR/adaptive_run.log"
[ -f "sub_test.log" ] && cp sub_test.log "$WEB_DIR/standard_run.log"

# کپی نمودارها به وب‌سرور
cp "$NEW_PLOTS_DIR"/*.png "$WEB_DIR/" 2>/dev/null || true

echo "=================================================="
echo "[✔] New plots stored in: $NEW_PLOTS_DIR"
echo "[✔] Web server directory ($WEB_DIR) updated successfully!"
echo "=================================================="
