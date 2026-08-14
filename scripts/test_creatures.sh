#!/system/bin/sh
# GitHub Test Script 2: Quét Creatures & Aim Target
PIDS=$(pidof com.studiowildcard.wardrumstudios.ark || pgrep -f ark || pgrep -f wildcard)
if [ -n "$PIDS" ]; then
    echo "[✔ GITHUB SCRIPT] Đang soi Dino đang nhắm (PIDs: $PIDS)..."
    for PID in $PIDS; do
        grep -a -o 'APrimalDinoCharacter_[A-Za-z0-9_]*' /proc/$PID/mem 2>/dev/null | head -n 15
    done
else
    echo "[-] Chưa tìm thấy PID game ARK Mobile!"
fi
