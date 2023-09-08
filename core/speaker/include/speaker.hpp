#ifndef SPEAKER_H_
#define SPEAKER_H_

#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>

namespace speaker
{

    // vits模型配置
    struct ModelConfig
    {
        float maxWavValue = 32768.0f; // 最大音量
        uint32_t sampleRate = 16000;  // 采样率
        float lengthScale = 1.0f;     // 时长缩放
        float noiseScale = 0.667f;
        float noiseW = 0.8f;
        bool singleSpeaker = false; // 是否为单音色
    };

    // onnx模型会话
    struct ModelSession
    {
        Ort::Env env;
        Ort::Session session;
        Ort::AllocatorWithDefaultOptions allocator;
        Ort::SessionOptions options;

        ModelSession() : session(nullptr){};
    };

    // 模型对象
    struct Model
    {
        ModelConfig config;
        ModelSession session;
    };

    // 合成结果
    struct SynthesisResult
    {
        int inferDuration; // 推理时长
        int audioDuration; // 音频时长
    };

    // 加载模型
    void initialize(
        const std::string &modelPath,       // vits模型路径
        const ModelConfig &modelConfig,     // vits模型配置
        const uint16_t &numThreads,         // 推理线程数
        const std::string &audioDeviceName, // 音频设备名称
        const std::string &audioMixerName   // 音频混音器名称
    );

    // 设置音频设备音量
    void setVolume(
        const uint16_t &volume // 音频音量
    );

    // 合成语音
    void synthesize(
        std::vector<int64_t> &phonemeIds,  // 音素ID向量
        const uint16_t &speakerId,         // 音色ID
        const float &speechRate,           // 语速
        std::vector<int16_t> &audioBuffer, // 合成音频数据
        SynthesisResult &result            // 合成结果
    );

    void say(
        std::vector<int64_t> &phonemeIds, // 音素ID向量
        const uint16_t &speakerId,        // 音色ID
        const float &speechRate,          // 语速
        const bool &block,                // 播放是否阻塞
        SynthesisResult &result           // 合成结果
    );

}

#endif
