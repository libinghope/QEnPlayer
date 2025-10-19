#!/bin/bash

# 创建测试目录
mkdir -p test_files

# 生成一个简单的测试视频（5秒，带颜色渐变）
ffmpeg -y -f lavfi -i color=c=blue:s=640x480:d=5 -vf "drawtext=text='Test Video':fontcolor=white:fontsize=36:x=(w-text_w)/2:y=(h-text_h)/2" -c:v libx264 -pix_fmt yuv420p test_files/test_video.mp4

# 创建一个简单的WAV音频文件（5秒，单声道，44100Hz）
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=5" test_files/test_audio.wav

# 将音频添加到视频中
ffmpeg -y -i test_files/test_video.mp4 -i test_files/test_audio.wav -c:v copy -c:a aac test_files/test_video_with_audio.mp4

echo "测试视频创建完成：test_files/test_video_with_audio.mp4"
echo "请在EnPlayer中打开此视频并测试语音识别功能"