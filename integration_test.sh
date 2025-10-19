#!/bin/bash

echo "===== EnPlayer语音识别功能集成测试开始 ====="

# 确保测试文件存在
TEST_VIDEO="../test_files/test_video_with_audio.mp4"
if [ ! -f "$TEST_VIDEO" ]; then
    echo "错误: 测试视频不存在! 请先运行test_recognition.sh生成测试视频"
    exit 1
fi

# 停止可能正在运行的EnPlayer进程
pkill -f EnPlayer || true
sleep 2

# 启动EnPlayer应用程序
echo "启动EnPlayer应用程序..."
cd /Users/bing/workspace/EnPlayer/build
./EnPlayer > enplayer_test.log 2>&1 &
ENPLAYER_PID=$!
sleep 5

# 检查应用程序是否启动成功
if ! ps -p $ENPLAYER_PID > /dev/null; then
    echo "错误: EnPlayer启动失败!"
    exit 1
fi

echo "EnPlayer已启动 (PID: $ENPLAYER_PID)"

# 使用AppleScript模拟用户操作（在macOS上）
echo "等待5秒钟让应用程序完全启动..."
sleep 5

echo "尝试通过AppleScript控制EnPlayer..."

# 编写AppleScript来打开视频文件并开始语音识别
cat > test_commands.scpt << 'EOF'
-- 等待EnPlayer窗口出现
set maxWaitTime to 10
set startTime to current date

repeat until (exists window "EnPlayer") or ((current date) - startTime > maxWaitTime)
    delay 1
end repeat

if not (exists window "EnPlayer") then
    return "错误: 找不到EnPlayer窗口"
end if

tell application "System Events"
    tell process "EnPlayer"
        -- 点击打开文件菜单
        click menu item "打开文件..." of menu "文件" of menu bar 1
        delay 1
        
        -- 在打开文件对话框中输入文件路径
        keystroke "g" using {command down, shift down}
        delay 1
        keystroke "/Users/bing/workspace/EnPlayer/test_files/test_video_with_audio.mp4"
        delay 1
        keystroke return
        delay 2
        keystroke return
        delay 3
        
        -- 点击语音识别按钮（假设在菜单栏中）
        try
            click menu item "语音识别" of menu "工具" of menu bar 1
            return "成功: 已触发语音识别"
        on error
            return "错误: 无法找到语音识别菜单选项"
        end try
    end tell
end tell
EOF

# 执行AppleScript
osascript test_commands.scpt

# 等待语音识别完成
echo "等待语音识别完成（30秒）..."
sleep 30

# 检查日志中的语音识别输出
echo "检查识别日志..."
grep -A 10 "[SpeechRecognizer]" enplayer_test.log | tail -20

# 清理临时文件
rm test_commands.scpt

# 停止EnPlayer进程
echo "停止EnPlayer进程..."
kill $ENPLAYER_PID

# 提供测试结果建议
echo ""
echo "===== 测试完成 ====="
echo "请在应用程序界面检查是否成功显示识别结果"
echo "详细日志请查看: enplayer_test.log"
echo "建议手动验证："
echo "1. 打开EnPlayer"
echo "2. 打开测试文件: /Users/bing/workspace/EnPlayer/test_files/test_video_with_audio.mp4"
echo "3. 点击语音识别按钮"
echo "4. 检查是否成功识别并显示结果"