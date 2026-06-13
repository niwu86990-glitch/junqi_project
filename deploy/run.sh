#!/bin/sh
APP_ROOT=$(cd "$(dirname "$0")" && pwd)

export LD_LIBRARY_PATH=/opt/qt5.6.2/lib:/opt/opencv3/lib:/opt/tslib/lib:$LD_LIBRARY_PATH

# 诊断信息不变
echo "=== DIAG: framebuffer ==="
fbset -i 2>/dev/null || cat /sys/class/graphics/fb0/virtual_size 2>/dev/null
echo "=== DIAG: input devices ==="
ls -l /dev/input/event* 2>/dev/null
echo "=== DIAG: free memory ==="
free -m

# Pause the board test UI before Qt takes ownership of the framebuffer. Do not
# terminate hdmi_x210: on this board it also keeps display hardware initialized,
# and killing it can make Qt 5.6 crash while showing the first window.
pause_board_ui() {
    BOARD_UI_PIDS=""
    for NAME in qttest hdmi_x210; do
        PIDS=$(pidof "${NAME}" 2>/dev/null)
        [ -n "${PIDS}" ] || continue
        for PID in ${PIDS}; do
            echo "pausing pid=${PID} command=${NAME}"
            kill -STOP "${PID}" 2>/dev/null || true
            BOARD_UI_PIDS="${BOARD_UI_PIDS} ${PID}"
        done
    done
}

echo "=== DIAG: board display services ==="
pause_board_ui

export QT_QPA_PLATFORM_PLUGIN_PATH=/opt/qt5.6.2/plugins/platforms
export QT_QPA_PLATFORM=linuxfb
export QT_QPA_FB=/dev/fb0
export QT_QPA_FONTDIR=${APP_ROOT}/fonts
export QT_QPA_LINUXFB_NO_DOUBLE_BUFFER=1

cleanup_snapshots() {
    rm -f /tmp/snap_*.jpg
}

resume_board_ui() {
    for PID in ${BOARD_UI_PIDS}; do
        [ -d "/proc/${PID}" ] && kill -CONT "${PID}" 2>/dev/null || true
    done
}

cleanup_all() {
    cleanup_snapshots
    resume_board_ui
}

terminate_app() {
    if [ -n "${APP_PID}" ]; then
        kill "${APP_PID}" 2>/dev/null || true
        wait "${APP_PID}" 2>/dev/null || true
    fi
    exit 0
}

# Remove stale captures left by a power loss or forced termination. Captures
# from this run are removed again when the application exits normally.
cleanup_snapshots
trap cleanup_all EXIT
trap terminate_app HUP INT TERM

# Disable Linux console blanking and DPMS, then clear the previous frame.
printf '\033[9;0]\033[14;0]\033[13]' > /dev/tty0 2>/dev/null || true
setterm -blank 0 -powersave off -powerdown 0 > /dev/tty0 2>/dev/null || true
printf '\033[2J\033[H' > /dev/tty0 2>/dev/null || clear 2>/dev/null || true
if [ -w /dev/fb0 ]; then
    FB_SIZE=$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null)
    FB_BPP=$(cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null)
    FB_WIDTH=${FB_SIZE%,*}
    FB_HEIGHT=${FB_SIZE#*,}
    case "${FB_WIDTH}:${FB_HEIGHT}:${FB_BPP}" in
        *[!0-9:]*|::*|*:) ;;
        *)
            FB_BYTES=$((FB_WIDTH * FB_HEIGHT * FB_BPP / 8))
            dd if=/dev/zero of=/dev/fb0 bs=4096 \
               count=$(((FB_BYTES + 4095) / 4096)) 2>/dev/null || true
            ;;
    esac
fi

# 启动程序
cd ${APP_ROOT}
./junqi_gui --template-dir ./templates/ --camera /dev/video0 &
APP_PID=$!

wait "${APP_PID}"
APP_STATUS=$?
APP_PID=""
echo "[RUN] junqi_gui exited with status ${APP_STATUS}"
exit "${APP_STATUS}"
