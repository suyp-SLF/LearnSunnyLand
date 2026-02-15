#!/bin/bash

# 1. 获取路径
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"
OUTPUT_DIR="../../../assets/shaders"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir -p "$OUTPUT_DIR"
fi

echo "🚀 开始跨平台 Shader 编译..."

# 定义编译函数，减少重复代码
compile_shader() {
    local file=$1
    local name=$2
    local stage=$3 # vert 或 frag

    echo "编译: $file -> $name.spv"
    # 生成 SPIR-V
    glslangValidator -V "$file" -o "$OUTPUT_DIR/$name.spv"
    
    if [ $? -eq 0 ]; then
        echo "转换: $name.spv -> $name.msl (Metal)"
        # ⚡️ 核心步骤：将 SPV 转换为 MSL 源码文本
        spirv-cross "$OUTPUT_DIR/$name.spv" --msl --output "$OUTPUT_DIR/$name.msl"
    else
        echo "❌ $file 编译失败"
        exit 1
    fi
}

# 执行编译
compile_shader "sprite.vert" "sprite.vert" "vert"
compile_shader "sprite.frag" "sprite.frag" "frag"

echo "--------------------------------------"
echo "✅ 所有 Shader 处理完成！"
echo "产物目录已包含 .spv (Vulkan) 和 .msl (Metal/Mac)"
echo "--------------------------------------"