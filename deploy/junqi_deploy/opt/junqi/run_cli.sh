#!/bin/sh
# 军棋 CLI 命令行测试脚本（在 ARM 开发板上运行）
# 用法: sh run_cli.sh <图片路径>
# 示例: sh run_cli.sh /root/test.jpg

APP_ROOT=$(cd "$(dirname "$0")" && pwd)

export LD_LIBRARY_PATH=/opt/qt5.6.2/lib:/opt/opencv3/lib:/opt/tslib/lib:$LD_LIBRARY_PATH

if [ $# -lt 1 ]; then
    echo "用法: sh run_cli.sh <图片路径>"
    echo "示例: sh run_cli.sh /root/test.jpg"
    exit 1
fi

cd ${APP_ROOT}
exec ./junqi_cli "$1" ./templates/
