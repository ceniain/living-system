#!/bin/bash
# ============================================================
# 直播项目一键压测脚本
#0 功能：编译 -> 执行所有压测 -> 收集结果 -> 生成报告
# 用法：chmod +x run_all_stress.sh && ./run_all_stress.sh
# ============================================================

# ================= 配置区 =================
# 只需要改这里，cpp 文件不需要改！
SERVER_IP="192.168.179.129"      # 改成你的虚拟机IP
SERVER_PORT=8000               # 服务端端口
ROOM_NO=1234567890             # 改成你的房间号（只需改这一处！）
LIVE_SERVER_PATH="/root/LiveServer/LiveServer"  # 服务端程序路径

# 压测参数
CONN_TARGET=5000               # 连接数目标
USER_COUNT=1000                # 弹幕压测用户数
MSG_PER_USER=10                # 每用户消息数
COMP_USER_COUNT=500            # 综合压测用户数

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 自动获取脚本目录
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPORT_DIR="${SCRIPT_DIR}/reports"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="${REPORT_DIR}/stress_report_${TIMESTAMP}.txt"

# ================= 函数区 =================

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 清理旧文件权限问题
cleanup_old_files() {
    # 如果 reports 目录属于 root，需要提示用户清理
    if [ -d "$REPORT_DIR" ]; then
        local owner=$(stat -c '%U' "$REPORT_DIR" 2>/dev/null)
        if [ "$owner" = "root" ]; then
            log_warn "检测到 reports 目录属于 root 用户（之前用了 sudo）"
            log_warn "正在清理...可能需要输入密码"
            sudo rm -rf "$REPORT_DIR"
            mkdir -p "$REPORT_DIR"
        fi
    fi
    # 清理旧的输出文件
    rm -f "${SCRIPT_DIR}/conn_stress_output.txt"
    rm -f "${SCRIPT_DIR}/chat_stress_output.txt"
    rm -f "${SCRIPT_DIR}/comprehensive_output.txt"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖..."
    local missing=0
    for cmd in g++ ps netstat awk grep timeout; do
        if ! command -v $cmd &>/dev/null; then
            log_error "缺少依赖: $cmd"
            missing=1
        fi
    done
    if [ $missing -eq 1 ]; then
        log_error "请安装缺失的依赖: sudo apt-get install g++ net-tools procps"
        exit 1
    fi
    log_success "依赖检查通过"
}

# 编译所有压测程序
compile_stress_tests() {
    log_info "编译压测程序..."
    cd "${SCRIPT_DIR}"
    
    if g++ conn_stress.cpp -o conn_stress -pthread -O2 2>/dev/null; then
        log_success "conn_stress 编译成功"
    else
        log_error "conn_stress 编译失败"
        return 1
    fi
    
    if g++ chat_stress.cpp -o chat_stress -pthread -O2 2>/dev/null; then
        log_success "chat_stress 编译成功"
    else
        log_error "chat_stress 编译失败"
        return 1
    fi
    
    if g++ comprehensive_stress.cpp -o comprehensive_stress -pthread -O2 2>/dev/null; then
        log_success "comprehensive_stress 编译成功"
    else
        log_error "comprehensive_stress 编译失败"
        return 1
    fi
    
    log_success "所有压测程序编译完成"
    return 0
}

# 获取服务端PID
get_server_pid() {
    pgrep -f "LiveServer" | head -1
}

# 检查服务端是否运行
check_server_running() {
    local pid=$(get_server_pid)
    if [ -z "$pid" ]; then
        log_error "服务端未运行！请先启动 LiveServer"
        return 1
    fi
    log_success "服务端运行中 (PID: $pid)"
    return 0
}

# 获取服务端资源使用情况（修复权限问题）
get_server_stats() {
    local pid=$(get_server_pid)
    if [ -z "$pid" ]; then
        echo "服务端未运行"
        return
    fi
    
    # 尝试读取内存，如果失败则显示未知
    local mem_kb=$(cat /proc/$pid/status 2>/dev/null | grep VmRSS | awk '{print $2}')
    local mem_mb=0
    if [ -n "$mem_kb" ] && [ "$mem_kb" -gt 0 ] 2>/dev/null; then
        mem_mb=$((mem_kb / 1024))
    fi
    
    local fds=$(ls /proc/$pid/fd 2>/dev/null | wc -l)
    local connections=$(netstat -anp 2>/dev/null | grep ":${SERVER_PORT}" | grep ESTABLISHED | wc -l)
    
    echo "内存: ${mem_mb}MB | FD: ${fds} | 连接: ${connections}"
}

# 记录初始状态
record_initial_stats() {
    log_info "记录服务端初始状态..."
    local pid=$(get_server_pid)
    if [ -z "$pid" ]; then
        echo "初始内存: 未知" >> "$REPORT_FILE"
        echo "初始连接: 未知" >> "$REPORT_FILE"
        return
    fi
    
    local mem_kb=$(cat /proc/$pid/status 2>/dev/null | grep VmRSS | awk '{print $2}')
    local mem_mb=0
    if [ -n "$mem_kb" ] && [ "$mem_kb" -gt 0 ] 2>/dev/null; then
        mem_mb=$((mem_kb / 1024))
    fi
    
    echo "初始内存: ${mem_mb}MB" >> "$REPORT_FILE"
    echo "初始连接: $(netstat -anp 2>/dev/null | grep ":${SERVER_PORT}" | grep ESTABLISHED | wc -l)" >> "$REPORT_FILE"
}

# 执行连接数压测
run_conn_stress() {
    log_info "=== 开始连接数压测 ==="
    echo "" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    echo "测试一: TCP连接数压测" >> "$REPORT_FILE"
    echo "开始时间: $(date)" >> "$REPORT_FILE"
    echo "目标连接数: ${CONN_TARGET}" >> "$REPORT_FILE"
    echo "----------------------------------------" >> "$REPORT_FILE"
    
    local start_time=$(date +%s)
    
    # 前台运行，30秒超时
    ROOM_NO=${ROOM_NO} timeout 30 ./conn_stress > "${SCRIPT_DIR}/conn_stress_output.txt" 2>&1 || true
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    log_info "连接建立后服务端状态:"
    get_server_stats | tee -a "$REPORT_FILE"
    
    # 解析结果
    local success=$(grep "成功连接" "${SCRIPT_DIR}/conn_stress_output.txt" 2>/dev/null | sed 's/[^0-9]//g' | tail -1)
    local failed=$(grep "失败连接" "${SCRIPT_DIR}/conn_stress_output.txt" 2>/dev/null | sed 's/[^0-9]//g' | tail -1)
    [ -z "$success" ] && success=0
    [ -z "$failed" ] && failed=0
    
    echo "耗时: ${duration}秒" >> "$REPORT_FILE"
    echo "成功连接: ${success}" >> "$REPORT_FILE"
    echo "失败连接: ${failed}" >> "$REPORT_FILE"
    
    if [ $((success + failed)) -gt 0 ]; then
        local rate=$((success * 100 / (success + failed)))
        echo "成功率: ${rate}%" >> "$REPORT_FILE"
    fi
    
    echo "最终状态: $(get_server_stats)" >> "$REPORT_FILE"
    log_success "连接数压测完成: 成功${success}, 失败${failed}"
    sleep 2
}

# 执行弹幕压测
run_chat_stress() {
    log_info "=== 开始弹幕压测 ==="
    echo "" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    echo "测试二: 弹幕并发压测" >> "$REPORT_FILE"
    echo "开始时间: $(date)" >> "$REPORT_FILE"
    echo "模拟用户: ${USER_COUNT}" >> "$REPORT_FILE"
    echo "每用户消息: ${MSG_PER_USER}" >> "$REPORT_FILE"
    echo "总消息数: $((USER_COUNT * MSG_PER_USER))" >> "$REPORT_FILE"
    echo "----------------------------------------" >> "$REPORT_FILE"
    
    local start_time=$(date +%s)
    
    ROOM_NO=${ROOM_NO} ./chat_stress > "${SCRIPT_DIR}/chat_stress_output.txt" 2>&1
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 解析结果
    local total=$(grep "总消息数" "${SCRIPT_DIR}/chat_stress_output.txt" 2>/dev/null | head -1 | sed 's/[^0-9]//g')
    local success=$(grep "成功发送" "${SCRIPT_DIR}/chat_stress_output.txt" 2>/dev/null | tail -1 | sed 's/[^0-9]//g')
    local failed=$(grep "失败发送" "${SCRIPT_DIR}/chat_stress_output.txt" 2>/dev/null | tail -1 | sed 's/[^0-9]//g')
    local tps=$(grep "吞吐量" "${SCRIPT_DIR}/chat_stress_output.txt" 2>/dev/null | tail -1 | sed 's/[^0-9.]//g')
    
    [ -z "$total" ] && total=0
    [ -z "$success" ] && success=0
    [ -z "$failed" ] && failed=0
    [ -z "$tps" ] && tps="0"
    
    echo "耗时: ${duration}秒" >> "$REPORT_FILE"
    echo "总消息数: ${total}" >> "$REPORT_FILE"
    echo "成功发送: ${success}" >> "$REPORT_FILE"
    echo "失败发送: ${failed}" >> "$REPORT_FILE"
    echo "吞吐量: ${tps} 条/秒" >> "$REPORT_FILE"
    echo "最终状态: $(get_server_stats)" >> "$REPORT_FILE"
    
    log_success "弹幕压测完成: 成功${success}, 失败${failed}"
    sleep 2
}

# 执行综合压测
run_comprehensive_stress() {
    log_info "=== 开始综合场景压测 ==="
    echo "" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    echo "测试三: 综合场景压测" >> "$REPORT_FILE"
    echo "开始时间: $(date)" >> "$REPORT_FILE"
    echo "模拟用户: ${COMP_USER_COUNT}" >> "$REPORT_FILE"
    echo "----------------------------------------" >> "$REPORT_FILE"
    
    local start_time=$(date +%s)
    
    ROOM_NO=${ROOM_NO} ./comprehensive_stress > "${SCRIPT_DIR}/comprehensive_output.txt" 2>&1
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 解析结果（使用 grep -o 提取数字）
    local online=$(grep "在线用户" "${SCRIPT_DIR}/comprehensive_output.txt" 2>/dev/null | grep -o '[0-9]\+' | head -1)
    local joined=$(grep "加入房间" "${SCRIPT_DIR}/comprehensive_output.txt" 2>/dev/null | grep -o '[0-9]\+' | head -1)
    local chatted=$(grep "发送弹幕" "${SCRIPT_DIR}/comprehensive_output.txt" 2>/dev/null | grep -o '[0-9]\+' | head -1)
    local left=$(grep "退出房间" "${SCRIPT_DIR}/comprehensive_output.txt" 2>/dev/null | grep -o '[0-9]\+' | head -1)
    local failed=$(grep "失败" "${SCRIPT_DIR}/comprehensive_output.txt" 2>/dev/null | grep -o '[0-9]\+' | tail -1)
    
    [ -z "$online" ] && online=0
    [ -z "$joined" ] && joined=0
    [ -z "$chatted" ] && chatted=0
    [ -z "$left" ] && left=0
    [ -z "$failed" ] && failed=0
    
    echo "耗时: ${duration}秒" >> "$REPORT_FILE"
    echo "在线用户: ${online}" >> "$REPORT_FILE"
    echo "加入房间: ${joined}" >> "$REPORT_FILE"
    echo "发送弹幕: ${chatted}" >> "$REPORT_FILE"
    echo "退出房间: ${left}" >> "$REPORT_FILE"
    echo "失败: ${failed}" >> "$REPORT_FILE"
    echo "最终状态: $(get_server_stats)" >> "$REPORT_FILE"
    
    if [ "$online" -gt 0 ] 2>/dev/null; then
        local fail_rate=$((failed * 100 / online))
        echo "失败率: ${fail_rate}%" >> "$REPORT_FILE"
    fi
    
    log_success "综合压测完成: 在线${online}, 加入${joined}, 退出${left}, 失败${failed}"
    sleep 2
}

# 检查内存泄漏
check_memory_leak() {
    log_info "检查服务端内存..."
    local pid=$(get_server_pid)
    if [ -z "$pid" ]; then
        echo "内存检查: 服务端未运行" >> "$REPORT_FILE"
        return
    fi
    
    local mem_kb=$(cat /proc/$pid/status 2>/dev/null | grep VmRSS | awk '{print $2}')
    local mem_mb=0
    if [ -n "$mem_kb" ] && [ "$mem_kb" -gt 0 ] 2>/dev/null; then
        mem_mb=$((mem_kb / 1024))
    fi
    
    echo "" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    echo "内存检查" >> "$REPORT_FILE"
    echo "最终内存: ${mem_mb}MB" >> "$REPORT_FILE"
    
    local initial_mem=$(grep "初始内存" "$REPORT_FILE" 2>/dev/null | head -1 | sed 's/[^0-9]//g')
    [ -z "$initial_mem" ] && initial_mem=0
    
    if [ "$initial_mem" -gt 0 ] 2>/dev/null; then
        local diff=$((mem_mb - initial_mem))
        echo "内存增长: ${diff}MB" >> "$REPORT_FILE"
        if [ $diff -gt 200 ]; then
            log_warn "内存增长超过200MB，可能存在内存泄漏"
            echo "警告: 内存增长超过200MB" >> "$REPORT_FILE"
        else
            log_success "内存增长正常 (${diff}MB)"
            echo "内存增长正常" >> "$REPORT_FILE"
        fi
    fi
}

# 生成报告
generate_report() {
    log_info "生成压测报告..."
    
    echo "" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    echo "压测总结" >> "$REPORT_FILE"
    echo "完成时间: $(date)" >> "$REPORT_FILE"
    echo "========================================" >> "$REPORT_FILE"
    
    echo ""
    log_info "=== 压测报告 ==="
    cat "$REPORT_FILE"
    
    log_success "报告已保存至: $REPORT_FILE"
}

# 清理
cleanup() {
    log_info "清理压测进程..."
    pkill -f conn_stress 2>/dev/null
    pkill -f chat_stress 2>/dev/null
    pkill -f comprehensive_stress 2>/dev/null
    log_success "清理完成"
}

# ================= 主流程 =================

main() {
    echo ""
    log_info "=========================================="
    log_info "  直播项目一键压测脚本"
    log_info "=========================================="
    log_info "  房间号: ${ROOM_NO}"
    log_info "=========================================="
    echo ""
    
    # 清理旧文件
    cleanup_old_files
    
    # 创建报告目录
    mkdir -p "$REPORT_DIR"
    
    # 初始化报告
    echo "直播项目压力测试报告" > "$REPORT_FILE"
    echo "生成时间: $(date)" >> "$REPORT_FILE"
    echo "服务器: ${SERVER_IP}:${SERVER_PORT}" >> "$REPORT_FILE"
    echo "房间号: ${ROOM_NO}" >> "$REPORT_FILE"
    
    check_dependencies
    
    if ! check_server_running; then
        log_error "请先启动服务端再运行压测"
        exit 1
    fi
    
    record_initial_stats
    
    if ! compile_stress_tests; then
        log_error "编译失败，退出压测"
        exit 1
    fi
    
    run_conn_stress
    run_chat_stress
    run_comprehensive_stress
    
    check_memory_leak
    generate_report
    cleanup
    
    log_success "所有压测完成！"
    log_info "报告位置: $REPORT_FILE"
}

trap cleanup EXIT INT TERM
main "$@"
