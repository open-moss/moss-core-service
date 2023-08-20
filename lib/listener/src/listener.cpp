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

    void loadModels(std::string modelDirPath, std::string unitPath)
    {
        vad->loadModel(modelDirPath + "/vad.onnx", 1, 1);
        std::shared_ptr<wenet::DecodeResource> decodeResource = wenet::InitDecodeResource(modelDirPath, unitPath, numThreads);
        featurePipeline = std::make_shared<wenet::FeaturePipeline>(*featureConfig);
        decoder = std::make_shared<wenet::AsrDecoder>(featurePipeline, decodeResource, *decodeConfig);
    }

    void input(std::vector<float> buffer)
    {
        int numSamples = buffer.size();
        int windowSamples = vadWindowFrameSize * (sampleRate / 1000);
        
        if(numSamples < windowSamples)
        {
            throw std::runtime_error("number of samples is less than the required window samples.");
        }

        for (int j = 0; j < numSamples; j += windowSamples)
        {
            std::vector<float> r{&buffer[0] + j, &buffer[0] + j + windowSamples};
            vad->predict(
                r, [](int startTime)
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
                samplingData.insert(samplingData.end(), r.begin(), r.end());
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
                sampledData.data[i] = sampledData.data[i] * 32768 * 3;
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

            int audio_duration = sampledData.endTime - sampledData.startTime;

            std::cout << finalResult << std::endl;

            // // std::cout << build_message(0, "OK", {{"event", "decode_end"}, {"start_time", sampledData.startTime}, {"end_time", sampledData.endTime}, {"result", finalResult}, {"decodeDuration", decodeDuration}, {"audio_duration", audio_duration}, {"rtf", round((static_cast<float>(decodeDuration) / audio_duration) * 1000.0) / 1000.0}}) << std::endl;

            lock.unlock();
        }
    }

}