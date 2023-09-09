# MOSS Core Service

## 简介

moss-core-service是[OpenMOSS](https://github.com/open-moss)的上位机服务程序，它的所有组件都是端到端（End-To-End）的，适用于ARM架构的边缘计算设备部署（如RK3588平台），提供与用户基于外设的交互能力，同时用于与[下位机](https://github.com/open-moss/moss-controller-fireware)通信控制MOSS姿态等功能和采集传感器信息，以及与[中控服务](https://github.com/open-moss/moss-central-service)通信。

### 组件列表

- 🔊 moss-speaker 提供流式发声器，基于vits语音推理并播放

- 👂️ moss-listener 提供流式监听器，集成VAD语音活动检测和Wenet语音识别

- 👁️ moss-monitor 提供监视器，基于Yolov5实时图像识别分类 **（评估中）**

OpenMOSS交流群：238111072

| [了解OpenMOSS](https://github.com/open-moss) | [快速开始](#快速开始) | [模型获取](#模型获取) | [开发计划](#开发计划) |

## 快速开始

### 准备环境

#### Node.js环境

确保已部署可用的Node.js环境，版本需求：16+

#### 配置ALSA

通过 ```alplay -l``` 可查看已识别的声卡设备

编辑 ```~/.asoundrc``` 写入以下内容，我的声卡设备是hw3,0，根据实际更换您的设备
```
pcm.!default {
        type asym
        playback.pcm {
                type plug
                slave.pcm "hw:3,0"
        }
        capture.pcm {
                type plug
                slave.pcm "hw:2,0"
        }
}

ctl.!default {
    type hw
    card 3
}
```

### 安装依赖

``` sh
sh install-deps.sh
npm install
```

### 启动服务

``` sh
npm start
```

## 模型获取

可以在以下链接中下载已经转换好的模型

### speaker模型

[moss-vits-onnx-model](https://huggingface.co/OpenMOSS/moss-vits-onnx-model) 

模型目录：models/speaker/

### listener模型

[moss-listener-models](https://huggingface.co/OpenMOSS/moss-listener-models)

vad模型来源：[silero-vad](https://github.com/snakers4/silero-vad)

encoder、decoder、ctc模型来源：[wenet-e2e](https://github.com/wenet-e2e/wenet) - wenetspeech数据集

模型目录：models/listener/

### monitor模型

此组件评估中，暂无模型

模型目录：models/monitor/

## 开发计划

- 增加monitor组件通过摄像头捕获图像使用Yolov5识别分类
- 与下位机固件进行协议对接
- 与中控服务进行协议对接
- 支持服务发现
- 优化模型推理性能，加速在边缘计算设备的推理速度
- 支持使用GPU或NPU加速推理
