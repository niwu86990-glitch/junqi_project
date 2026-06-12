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

# 路径核验无误，直接使用
export QT_QPA_PLATFORM_PLUGIN_PATH=/opt/qt5.6.2/plugins/platforms
export QT_QPA_PLATFORM=linuxfb
export QT_QPA_FB=/dev/fb0
export QT_QPA_FONTDIR=${APP_ROOT}/fonts
export QT_QPA_LINUXFB_NO_DOUBLE_BUFFER=1

# 清除控制台文字和上一帧画面，再显示 Qt 窗口。
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
exec ./junqi_gui --template-dir ./templates/ --camera /dev/video0
