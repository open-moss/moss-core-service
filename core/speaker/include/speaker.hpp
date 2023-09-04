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
        int sampleRate = 16000;       // 采样率
        float lengthScale = 1.0f;     // 时长缩放
        float noiseScale = 0.667f;
        float noiseW = 0.8f;
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
    void loadModel(
        const std::string &modelPath, // vits模型路径
        const ModelConfig &modelConfig,     // vits模型配置
        int16_t numThreads            // 推理线程数
    );

    // 合成语音
    void synthesize(
        std::vector<int64_t> &phonemeIds,  // 音素ID向量
        int16_t speakerId,                 // 音色ID
        float speechRate,                  // 语速
        std::vector<int16_t> &audioBuffer, // 合成后音频
        SynthesisResult &result            // 合成结果
    );

}

#endif
