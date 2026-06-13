#!/bin/sh
# Temporary board-side data collection tool.

APP_ROOT=$(cd "$(dirname "$0")" && pwd)
OUTPUT_DIR=/root/junqi_test_samples

export LD_LIBRARY_PATH=/opt/qt5.6.2/lib:/opt/opencv3/lib:/opt/tslib/lib:$LD_LIBRARY_PATH

mkdir -p "${OUTPUT_DIR}"
rm -f "${OUTPUT_DIR}"/sample_*.jpg "${OUTPUT_DIR}"/results.csv
rm -f /tmp/snap_*.jpg

echo "请先退出军棋 GUI，确保 /dev/video0 没有被占用。"
echo "采样结果将保存到 ${OUTPUT_DIR}"

cd "${APP_ROOT}"
exec ./junqi_capture_test ./templates "${OUTPUT_DIR}" /dev/video0 30
