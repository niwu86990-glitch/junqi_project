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

# 启动程序
cd ${APP_ROOT}
exec ./junqi_gui --template-dir ./templates/ --camera /dev/video0
