#!/system/bin/sh
# GitHub Test Script 1: Quét Spawn Zones
PIDS=$(pidof com.studiowildcard.wardrumstudios.ark || pgrep -f ark || pgrep -f wildcard)
if [ -n "$PIDS" ]; then
    echo "[✔ GITHUB SCRIPT] Đang quét Spawn Zones (PIDs: $PIDS)..."
    for PID in $PIDS; do
        grep -a -o 'DinoSpawnEntries_[A-Za-z0-9_]*' /proc/$PID/mem 2>/dev/null | sort -u
    done
else
    echo "[-] Chưa tìm thấy PID game ARK Mobile!"
fi
