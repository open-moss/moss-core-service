#ifndef SPEAKER_H_
#define SPEAKER_H_

#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>

#include "json.hpp"

using json = nlohmann::json;

namespace speaker
{

    // vits模型配置
    struct ModelConfig
    {
        int numSpeakers = 1;      // 音色个数
        int sampleRate = 16000;   // 采样率
        int64_t speakerId;        // 音色ID
        float lengthScale = 1.0f; // 时长缩放
        float noiseScale = 0.667f;
        float noiseW = 0.8f;
        int sampleWidth = 2; // 采样宽度
        int channels = 1;    // 通道数
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
        double inferDuration;  // 推理时长
        double audioDuration;  // 音频时长
        double realTimeFactor; // 实时率
    };

    // 获取版本
    std::string getVersion();

    // 加载模型
    void loadModel(
        std::string modelPath,       // vits模型路径
        std::string modelConfigPath, // vits模型配置路径
        int16_t numThreads          // 推理线程数
    );

    // 合成语音
    void synthesize(
        std::vector<int64_t> &phonemeIds,  // 音素ID向量
        float speechRate,                  // 语速
        std::vector<int16_t> &audioBuffer, // 合成后音频
        SynthesisResult &result            // 合成结果
    );

}

#endif
