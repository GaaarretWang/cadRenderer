#!/bin/bash

set -e

# 解析参数
MODE="normal"  # normal, save, compare
for arg in "$@"; do
    case $arg in
        --save)    MODE="save"; shift ;;
        --compare) MODE="compare"; shift ;;
        --help)
            echo "用法: bash test_all_scenes.sh [选项]"
            echo ""
            echo "选项:"
            echo "  --save      保存所有场景的渲染结果为参考图"
            echo "  --compare   将当前渲染结果与参考图对比"
            echo "  (无参数)    仅检查程序是否正常退出"
            exit 0
            ;;
    esac
done

echo "=== 开始测试所有场景 ==="
echo "测试模式: $MODE"
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

# 统计结果
pass_count=0
fail_count=0
total_count=${#all_scenes[@]}

echo "=== 测试所有场景 ==="
echo ""

for scene_id in "${all_scenes[@]}"; do
    scene_name="${scenes[$scene_id]}"
    echo "测试场景 $scene_id: $scene_name"
    echo "----------------------------------------"

    # 创建日志文件
    log_file="/tmp/scene_test_${scene_id}_$(date +%s).log"

    # 构建额外参数
    extra_args=""
    if [ "$MODE" = "save" ]; then
        extra_args="--save-ref"
    elif [ "$MODE" = "compare" ]; then
        extra_args="--compare-ref"
    fi

    # 运行程序，只渲染2帧
    echo "运行命令: (cd build && ./Rendering --scene $scene_id --frames 2 $extra_args)"
    sh -c "cd build && ./Rendering --scene \"$scene_id\" --frames 2 $extra_args" 2>&1 | tee "$log_file"
    exit_code=${PIPESTATUS[0]}

    echo "程序退出码: $exit_code"

    # 检查是否有"Program stopped"字样
    scene_pass=true
    if grep -q "Program stopped" "$log_file"; then
        echo "✓ 场景 $scene_id ($scene_name) 正常退出 (找到 'Program stopped' 字样)"
    else
        echo "✗ 场景 $scene_id ($scene_name) 未找到 'Program stopped' 字样"
        scene_pass=false
    fi

    # 对比模式下检查PASS/FAIL
    if [ "$MODE" = "compare" ]; then
        if grep -q "^PASS:" "$log_file"; then
            echo "✓ 场景 $scene_id ($scene_name) 对比通过"
        elif grep -q "^FAIL:" "$log_file"; then
            echo "✗ 场景 $scene_id ($scene_name) 对比失败"
            scene_pass=false
        else
            echo "⚠ 场景 $scene_id ($scene_name) 无对比结果"
            scene_pass=false
        fi
    fi

    # 保存模式下检查参考图是否保存成功
    if [ "$MODE" = "save" ]; then
        if grep -q "Reference saved:" "$log_file"; then
            echo "✓ 场景 $scene_id ($scene_name) 参考图已保存"
        else
            echo "✗ 场景 $scene_id ($scene_name) 参考图保存失败"
            scene_pass=false
        fi
    fi

    # 检查是否有错误信息
    if grep -q -i "错误\|error\|failed\|异常\|exception" "$log_file"; then
        echo "⚠ 场景 $scene_id ($scene_name) 输出中包含错误信息:"
        grep -i "错误\|error\|failed\|异常\|exception" "$log_file" | head -5
    fi

    if $scene_pass; then
        pass_count=$((pass_count + 1))
    else
        fail_count=$((fail_count + 1))
    fi

    echo ""

    # 保留日志文件供后续检查
    echo "详细日志保存到: $log_file"
    echo ""
done

echo "=== 测试完成 ==="
echo ""
echo "结果: $pass_count/$total_count 通过, $fail_count/$total_count 失败"

if [ "$MODE" = "save" ]; then
    echo ""
    echo "参考图保存目录: test_references/"
    ls -la test_references/ 2>/dev/null || echo "(目录不存在)"
fi

echo ""
echo "提示: 如需查看详细日志，请检查 /tmp/scene_test_*.log 文件"

# 如果有失败场景，返回非0退出码
if [ $fail_count -gt 0 ]; then
    exit 1
fi
