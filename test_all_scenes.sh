#!/bin/bash

set -e

echo "=== 开始测试所有场景 ==="
echo "当前目录: $(pwd)"
echo ""

# 编译函数
compile_if_needed() {
    if [ ! -f "build/Rendering" ]; then
        echo "未找到 build/Rendering 程序，开始编译..."
        echo ""

        # 创建build目录
        if [ ! -d "build" ]; then
            echo "创建 build 目录..."
            mkdir -p build
        fi

        # 进入build目录编译
        cd build
        echo "运行 cmake ..."
        cmake ..

        echo "运行 make ..."
        make -j$(nproc)

        # 返回原目录
        cd ..

        if [ ! -f "build/Rendering" ]; then
            echo "错误: 编译后仍未找到 build/Rendering 程序"
            exit 1
        fi

        echo "编译成功: build/Rendering 已生成"
        echo ""
    else
        echo "找到程序: build/Rendering"
        echo ""
    fi
}

# 检查并编译程序
compile_if_needed

# 所有场景列表
declare -A scenes=(
    ["0"]="airplane_parts"
    ["1"]="dashboard"
    ["2"]="spheres"
    ["3"]="whole_engine"
    ["4"]="helicopter_engine"
    ["5"]="cockpit"
)

# 测试所有6个场景
all_scenes=("0" "1" "2" "3" "4" "5")

echo "=== 测试所有场景 ==="
echo ""

for scene_id in "${all_scenes[@]}"; do
    scene_name="${scenes[$scene_id]}"
    echo "测试场景 $scene_id: $scene_name"
    echo "----------------------------------------"

    # 创建日志文件
    log_file="/tmp/scene_test_${scene_id}_$(date +%s).log"

    # 运行程序，只渲染2帧
    echo "运行命令: (cd build && ./Rendering --scene $scene_id --frames 2)"
    sh -c "cd build && ./Rendering --scene \"$scene_id\" --frames 2" 2>&1 | tee "$log_file"
    exit_code=${PIPESTATUS[0]}

    echo "程序退出码: $exit_code"

    # 检查是否有"Program stopped"字样
    if grep -q "Program stopped" "$log_file"; then
        echo "✓ 场景 $scene_id ($scene_name) 正常退出 (找到 'Program stopped' 字样)"
    else
        echo "✗ 场景 $scene_id ($scene_name) 未找到 'Program stopped' 字样"
    fi

    # 检查是否有错误信息
    if grep -q -i "错误\|error\|failed\|异常\|exception" "$log_file"; then
        echo "⚠ 场景 $scene_id ($scene_name) 输出中包含错误信息:"
        grep -i "错误\|error\|failed\|异常\|exception" "$log_file" | head -5
    fi

    # 检查是否有警告信息
    if grep -q -i "警告\|warning" "$log_file"; then
        echo "⚠ 场景 $scene_id ($scene_name) 输出中包含警告信息:"
        grep -i "警告\|warning" "$log_file" | head -5
    fi

    echo ""

    # 保留日志文件供后续检查
    echo "详细日志保存到: $log_file"
    echo ""
done

echo "=== 测试完成 ==="
echo ""
echo "所有场景测试完成！"
echo "提示: 如需查看详细日志，请检查 /tmp/scene_test_*.log 文件"