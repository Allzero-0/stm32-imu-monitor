#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  STM32F103C8T6 固件构建脚本（不依赖 make，直接用 bash + arm-none-eabi-gcc）
#
#  用法（Git Bash / Linux / macOS）：
#      ./build.sh            # 编译，生成 build/fw.elf fw.hex fw.bin
#      ./build.sh clean      # 清理
#
#  Windows 上若使用 ARM GCC 不在 PATH 中，请先设置环境变量：
#      export ARMGCC_HOME="C:/Users/lenovo/.workbuddy/binaries/armtc/gcc-arm-none-eabi-10.3-2021.10"
# ---------------------------------------------------------------------------
set -e

# ---- 工具链路径 ----
if [ -z "$ARMGCC_HOME" ]; then
    ARMGCC_HOME="C:/Users/lenovo/.workbuddy/binaries/armtc/gcc-arm-none-eabi-10.3-2021.10"
fi
CC="$ARMGCC_HOME/bin/arm-none-eabi-gcc"
OBJCOPY="$ARMGCC_HOME/bin/arm-none-eabi-objcopy"
SIZE="$ARMGCC_HOME/bin/arm-none-eabi-size"

if [ ! -f "$CC" ]; then
    echo "[ERROR] 找不到交叉编译器: $CC"
    echo "        请设置 ARMGCC_HOME 环境变量指向 gcc-arm-none-eabi 根目录"
    exit 1
fi

# ---- 目录 ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
SRC_DIR="src"
BUILD_DIR="build"
LDSCRIPT="linker/stm32f103c8t6.ld"

if [ "$1" == "clean" ]; then
    rm -rf "$BUILD_DIR"
    echo "cleaned."
    exit 0
fi

mkdir -p "$BUILD_DIR"

# ---- 编译选项 ----
CPU_FLAGS="-mcpu=cortex-m3 -mthumb -mfloat-abi=soft"
CFLAGS="-std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common -g3"
ASFLAGS="-x assembler-with-cpp"
LDFLAGS="-T $LDSCRIPT -Wl,--gc-sections -Wl,-Map=$BUILD_DIR/fw.map -Wl,--print-memory-usage"
LDFLAGS="$LDFLAGS --specs=nano.specs"

SOURCES=(
    "$SRC_DIR/startup_stm32f103xb.s"
    "$SRC_DIR/system.c"
    "$SRC_DIR/usart.c"
    "$SRC_DIR/soft_i2c.c"
    "$SRC_DIR/mpu6050.c"
    "$SRC_DIR/adc_dma.c"
    "$SRC_DIR/tim.c"
    "$SRC_DIR/imu.c"
    "$SRC_DIR/protocol.c"
    "$SRC_DIR/app.c"
    "$SRC_DIR/main.c"
)

OBJS=()

echo "=============================================="
echo " Building STM32F103 IMU Monitor"
echo "=============================================="

for src in "${SOURCES[@]}"; do
    name="$(basename "$src")"
    obj="$BUILD_DIR/${name%.*}.o"
    echo "  CC  $name"
    if [[ "$src" == *.s ]]; then
        "$CC" $CPU_FLAGS $ASFLAGS -I"$SRC_DIR" -c "$src" -o "$obj"
    else
        "$CC" $CPU_FLAGS $CFLAGS -I"$SRC_DIR" -c "$src" -o "$obj"
    fi
    OBJS+=("$obj")
done

echo "  LD  fw.elf"
"$CC" $CPU_FLAGS $LDFLAGS "${OBJS[@]}" -lm -lc -lgcc -o "$BUILD_DIR/fw.elf"

echo "  HEX fw.hex"
"$OBJCOPY" -O ihex "$BUILD_DIR/fw.elf" "$BUILD_DIR/fw.hex"
echo "  BIN fw.bin"
"$OBJCOPY" -O binary "$BUILD_DIR/fw.elf" "$BUILD_DIR/fw.bin"

echo "----------------------------------------------"
"$SIZE" "$BUILD_DIR/fw.elf"
echo "----------------------------------------------"
echo "输出文件："
echo "  $BUILD_DIR/fw.elf   (用于 GDB 调试)"
echo "  $BUILD_DIR/fw.hex   (用于 ST-Link Utility / FlyMcu 串口下载)"
echo "  $BUILD_DIR/fw.bin   (用于 st-flash / OpenOCD, 烧录地址 0x08000000)"
echo "构建成功。"
