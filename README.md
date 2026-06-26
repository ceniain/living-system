# 直播系统

基于 FFmpeg + SDL2 + epoll 的实时直播系统，支持主播推流、观众拉流、弹幕互动。

## 技术栈

| 模块 | 技术 |
|------|------|
| **音视频处理** | FFmpeg（编解码、格式转换） |
| **音频播放** | SDL2（跨平台音频设备） |
| **图像采集** | OpenCV（摄像头、桌面截图） |
| **网络框架** | epoll + 线程池、自定义 TCP 协议 |
| **数据库** | MySQL（用户/房间管理） |
| **UI 框架** | Qt5（信号槽跨线程通信） |

## 架构

```
主播端 (Qt + OpenCV + FFmpeg)
    │
    │ RTMP 推流 (H.264 + AAC)
    ▼
服务端 (epoll + 线程池 + MySQL)
    │
    │ 自定义 TCP 协议转发
    ▼
观众端 (Qt + FFmpeg + SDL2)
```

## 核心功能

- [x] 摄像头 / 桌面采集 + 画中画合成
- [x] H.264 + AAC 编码推流
- [x] 音视频同步（PTS/DTS，音频主时钟）
- [x] 房间管理 + 弹幕广播 + 礼物系统
- [x] 心跳检测 + 自动清理僵尸连接
- [x] 线程池自动扩缩容
- [x] 压测验证（连接数 / 弹幕 / 综合场景）

## 项目结构

```
├── LiveServer/          # 服务端
│   ├── src/             # epoll 网络框架、线程池、MySQL、业务逻辑
│   ├── include/         # 头文件
│   └── stress_test/     # 压测脚本
├── MediaPlayer/         # 观众端（播放器）
│   ├── videoplayer.cpp  # 音视频解码、同步、渲染
│   ├── TcpClient.cpp    # 网络通信
│   └── opengl/          # OpenGL 渲染
├── recorder/            # 主播端（录制推流）
│   ├── picinpic_read.cpp # 画中画采集
│   ├── savevideofilethread.cpp # 编码推流
│   └── audio_read.cpp   # 音频采集
└── MusicPlayer/         # 音乐播放器（PacketQueue 实现）
```

## 快速开始

### 服务端

```bash
cd LiveServer/LiveServer
qmake LiveServer.pro
make
./LiveServer
```

### 主播端

```bash
cd recorder/VideoRecorder
qmake VideoRecorder.pro
make
./VideoRecorder
```

### 观众端

```bash
cd MediaPlayer
qmake MediaPlayer.pro
make
./MediaPlayer
```

## 压测

```bash
cd LiveServer/LiveServer/stress_test
chmod +x run_all_stress.sh
./run_all_stress.sh
```

## 依赖

- FFmpeg 4.2.2
- SDL2 2.0.10
- OpenCV
- Qt 5.12
- MySQL 5.7+
