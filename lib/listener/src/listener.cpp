#include <iomanip>
#include <utility>
#include <thread>
#include <queue>

#include "decoder/params.h"
#include "utils/string.h"
#include "utils/timer.h"
#include "utils/utils.h"
#include "vad/vad.h"

#include "listener.hpp"

namespace listener
{

#ifdef LISTENER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
    const std::string VERSION = STR(LISTENER_VERSION);
#else
    const std::string VERSION = "";
#endif

    std::shared_ptr<VadIterator> vad;
    std::shared_ptr<wenet::DecodeOptions> decodeConfig;
    std::shared_ptr<wenet::FeaturePipelineConfig> featureConfig;
    std::shared_ptr<wenet::FeaturePipeline> featurePipeline;
    std::shared_ptr<wenet::AsrDecoder> decoder;
    std::queue<SampledData> decodeQueue;
    std::vector<float> samplingData;
    std::function<void(const DecodeResult&)> callback;
    int64_t samplingStartTime = 0;
    bool isSampling = false;
    std::mutex mtx;
    std::condition_variable cv;
    int sampleRate = 16000;
    int vadWindowFrameSize = 64;
    double vadThreshold = 0.6f;
    int vadMaxSamplingDuration = 180000;
    int16_t numThreads = 1;

    void processDecode();

    std::string getVersion()
    {
        return VERSION;
    }

    void init(int _sampleRate, int _vadWindowFrameSize, double _vadThreshold, int _vadMaxSamplingDuration, int16_t chunkSize, int16_t _numThreads)
    {
        google::InitGoogleLogging("");
        sampleRate = _sampleRate;
        vadWindowFrameSize = _vadWindowFrameSize;
        vadThreshold = _vadThreshold;
        vadMaxSamplingDuration = _vadMaxSamplingDuration;
        numThreads = _numThreads;
        vad = std::make_shared<VadIterator>(sampleRate, vadWindowFrameSize, vadThreshold, 0, 0);
        decodeConfig = wenet::InitDecodeOptionsFromFlags();
        decodeConfig->chunk_size = chunkSize;
        featureConfig = wenet::InitFeaturePipelineConfigFromFlags();
        featureConfig->sample_rate = sampleRate;
        std::thread decodeThread(processDecode);
        decodeThread.detach();
    }

    void loadModels(const std::string &modelDirPath, const std::string &unitPath)
    {
        vad->loadModel(modelDirPath + "/vad.onnx", 1, 1);
        std::shared_ptr<wenet::DecodeResource> decodeResource = wenet::InitDecodeResource(modelDirPath, unitPath, numThreads);
        featurePipeline = std::make_shared<wenet::FeaturePipeline>(*featureConfig);
        decoder = std::make_shared<wenet::AsrDecoder>(featurePipeline, decodeResource, *decodeConfig);
    }

    void input(const std::string &raw)
    {
        int numSamples = raw.size() / 2;
        int windowSamples = vadWindowFrameSize * (sampleRate / 1000);

        if (numSamples < windowSamples)
        {
            throw std::runtime_error("number of samples is less than the required window samples.");
        }

        for (int j = 0; j < numSamples; j += windowSamples)
        {
            std::vector<float> inputData;
            for (int i = j * 2; i < (j + windowSamples) * 2; i += 2)
            {
                char lowByte = raw[i];
                char highByte = raw[i + 1];
                int16_t int16Value = static_cast<int16_t>(lowByte) | (static_cast<int16_t>(highByte) << 8);
                inputData.push_back(static_cast<float>(int16Value) / 32768);
            }

            vad->predict(
                inputData, [](int startTime)
                {
                    samplingData.clear();
                    isSampling = true;
                    samplingStartTime = startTime; },
                [](int endTime)
                {
                    decodeQueue.push({samplingData, samplingStartTime, endTime});
                    cv.notify_one();
                    samplingData.clear();
                    isSampling = false;
                });
            if (isSampling)
            {
                samplingData.insert(samplingData.end(), inputData.begin(), inputData.end());
                int currentSamplingDuration = vad->getCurrentTime() - samplingStartTime;
                if (currentSamplingDuration >= vadMaxSamplingDuration)
                {
                    decodeQueue.push({samplingData, samplingStartTime, samplingStartTime + currentSamplingDuration});
                    samplingStartTime = samplingStartTime + currentSamplingDuration;
                    cv.notify_one();
                    samplingData.clear();
                }
            }
        }
    }

    void output(const std::function<void(const DecodeResult&)>& _callback)
    {
        callback = _callback;
    }

    void processDecode()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, []
                    { return !decodeQueue.empty(); });
            SampledData sampledData = decodeQueue.front();
            decodeQueue.pop();

            for (int i = 0; i < sampledData.data.size(); i++)
            {
                sampledData.data[i] = sampledData.data[i] * 32768;
            }

            decoder->Reset();
            featurePipeline->Reset();
            featurePipeline->AcceptWaveform(sampledData.data.data(), sampledData.data.size());
            featurePipeline->set_input_finished();

            int decodeDuration = 0;
            std::string finalResult;
            while (true)
            {
                wenet::Timer timer;
                wenet::DecodeState state = decoder->Decode();
                if (state == wenet::DecodeState::kEndFeats)
                {
                    decoder->Rescoring();
                }
                int chunk_decode_time = timer.Elapsed();
                decodeDuration += chunk_decode_time;
                if (state == wenet::DecodeState::kEndFeats)
                {
                    break;
                }
            }
            if (decoder->DecodedSomething())
            {
                finalResult.append(decoder->result()[0].sentence);
            }
            if (finalResult.empty())
            {
                continue;
            }

            int audioDuration = sampledData.endTime - sampledData.startTime;

            if(callback)
            {
                float realTimeFactor = round((static_cast<float>(decodeDuration) / audioDuration) * 1000.0) / 1000.0;
                callback({ sampledData.startTime, sampledData.endTime, finalResult, decodeDuration, audioDuration, realTimeFactor });
            }

            lock.unlock();
        }
    }

}