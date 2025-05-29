#!/bin/bash
# 设置错误时立即退出
set -e
# 如果build目录存在，删除它
if [ -d "build" ]; then
    rm -rf build
fi

mkdir build
cd build

# 检查cmake执行结果
if ! cmake ..; then
    echo "CMake configuration failed"
    exit 1
fi

# 检查make执行结果
if ! make; then
    echo "Build failed"
    exit 1
fi


./unit_test
./perf_test