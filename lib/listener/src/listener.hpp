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
        std::string finalResult;
        int decodeDuration;
    };

    struct SampledData {
        std::vector<float> data;
        int64_t startTime;
        int64_t endTime;
    };

    // 获取版本
    std::string getVersion();

    void init(int sampleRate, int vadWindowFrameSize, double vadThreshold, int vadMaxSamplingDuration, int16_t chunkSize, int16_t numThreads);

    void loadModels(std::string modelDirPath, std::string unitPath);

    void input(std::vector<float> buffer);

}

#endif
