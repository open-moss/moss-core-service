#ifndef LISTENER_H_
#define LISTENER_H_

#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>

namespace listener
{

    struct DecodeResult {
        int64_t startTime;
        int64_t endTime;
        std::string result;
        int decodeDuration;
        int audioDuration;
        float realTimeFactor;
    };

    struct SampledData {
        std::vector<float> data;
        int64_t startTime;
        int64_t endTime;
    };

    // 获取版本
    std::string getVersion();

    void init(int sampleRate, int vadWindowFrameSize, double vadThreshold, int vadMaxSamplingDuration, float samplingAmplificationFactor, int16_t chunkSize, int16_t numThreads);

    void loadModels(const std::string &modelDirPath, const std::string &unitPath);

    void input(const std::string &raw);

    void output(const std::function<void(const DecodeResult&)>& callback);

}

#endif
