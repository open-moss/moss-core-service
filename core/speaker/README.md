# MOSS Speaker

## 简介

moss-speaker是[OpenMOSS](https://github.com/open-moss)的端到端（End-To-End）发音组件，它属于[moss-core-service](https://github.com/open-moss/moss-core-service)的一部分，用于合成《流浪地球》MOSS声线语音并发声。

基于C++借助ONNXRuntime进行vits语音推理，使用Node.js N-API封装为模块。

OpenMOSS交流群：238111072

| [了解OpenMOSS](https://github.com/open-moss) | [快速开始](#快速开始) | [模型获取](#模型获取) | [开发计划](#开发计划) |

## 功能概述

- 🚀 适用于较低算力的ARM架构设备，可以在边缘计算设备部署

- 🚀 推理后端使用ONNXRuntime，RK3588平台4线程实测RTF可达0.4

- 🚀 支持Node.js调用，使用简单

- 🚀 使用ALSA音频库发声

- 🚀 支持其它单音色或多音色vits模型

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

### 构建模块

如果希望在此目录构建模块，请执行

``` sh
npm install
```

### 运行Demo

``` sh
node demo.js
```

demo.js 内容如下
``` javascript
import { Speaker } from "./index.js";

const speaker = new Speaker({
    modelPath: "../../models/speaker/moss.onnx",  // onnx模型文件
    modelConfigPath: "../../models/speaker/moss.json",  // 模型配置文件
    numThreads: 4,  // 推理线程数
    singleSpeaker: true  // 是否单音色模型
});

await speaker.setVolume(100);  // 设置音量为100%

const result = await speaker.say("让人类永远保持理智，的确是一种奢求", {
    speechRate: 0.8,  // 语音语速
    block: true  // 播放是否阻塞
});  // 合成语音并播放

console.log(result);  // 合成结果
```

## 模型获取

可以在以下链接中下载已经转换好的模型

[moss-vits-onnx-model](https://huggingface.co/OpenMOSS/moss-vits-onnx-model) 

## 开发计划

- 针对不同文本场景自动处理读法（如数字读法）
- 支持根据内容自动断句流式合成与播放
- 通过混音器将进行混响
- 支持使用GPU或NPU加速推理
