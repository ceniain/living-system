#!/bin/bash
# ============================================================
# 单个压测运行脚本
# 用法：./run_single_test.sh [conn|chat|comprehensive]
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 颜色
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 检查参数
if [ -z "$1" ]; then
    echo "用法: $0 [conn|chat|comprehensive]"
    echo ""
    echo "  conn         - TCP连接数压测（5000连接）"
    echo "  chat         - 弹幕并发压测（1000用户x10消息）"
    echo "  comprehensive - 综合场景压测（登录->加房->弹幕->退出）"
    echo ""
    echo "示例:"
    echo "  $0 conn"
    echo "  $0 chat"
    echo "  $0 comprehensive"
    exit 1
fi

# 编译所有压测程序
log_info "编译压测程序..."
cd "${SCRIPT_DIR}"

g++ conn_stress.cpp -o conn_stress -pthread -O2 2>/dev/null && log_success "conn_stress 编译成功" || log_error "conn_stress 编译失败"
g++ chat_stress.cpp -o chat_stress -pthread -O2 2>/dev/null && log_success "chat_stress 编译成功" || log_error "chat_stress 编译失败"
g++ comprehensive_stress.cpp -o comprehensive_stress -pthread -O2 2>/dev/null && log_success "comprehensive_stress 编译成功" || log_error "comprehensive_stress 编译失败"

echo ""

# 根据参数运行对应测试
case "$1" in
    conn)
        log_info "=== 运行 TCP连接数压测 ==="
        ./conn_stress
        ;;
    chat)
        log_info "=== 运行 弹幕并发压测 ==="
        ./chat_stress
        ;;
    comprehensive)
        log_info "=== 运行 综合场景压测 ==="
        ./comprehensive_stress
        ;;
    *)
        log_error "未知参数: $1"
        echo "用法: $0 [conn|chat|comprehensive]"
        exit 1
        ;;
esac
