set -e  # 任何命令失败立即退出

mkdir -p build
cd build
cmake ..
if make -j$(nproc); then
    echo "编译成功，准备运行程序..."
    ./Rendering
else
    echo "编译失败，请检查错误"
    exit 1
fi