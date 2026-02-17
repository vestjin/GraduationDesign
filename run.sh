#!/bin/bash

PORT=8080

echo "🔍 检查端口 $PORT 是否被占用..."
pid=$(sudo lsof -t -i:$PORT 2>/dev/null)
if [ -n "$pid" ]; then
    echo "⚠️  端口 $PORT 被进程 $pid 占用，正在强制关闭..."
    sudo kill -9 $pid
    sleep 1
fi

echo "🔨 编译项目中..."
make clean && make

if [ $? -ne 0 ]; then
    echo "❌ 编译失败，请检查错误"
    exit 1
fi

echo "✅ 编译成功，启动后端服务器（后台运行）..."
./bin/cloud_disk &

SERVER_PID=$!
echo "🚀 服务器 PID: $SERVER_PID"
echo ""
echo "👉 请在另一个终端中执行以下命令启动前端服务："
echo "   cd $(pwd) && python3 -m http.server 8000"
echo ""
echo "🌐 访问地址：http://127.0.0.1:8000"
echo ""
echo "按 Ctrl+C 停止后端服务器"

# 等待服务器进程结束（前台阻塞，方便 Ctrl+C 终止）
wait $SERVER_PID