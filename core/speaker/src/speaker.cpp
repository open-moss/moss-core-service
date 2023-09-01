#include <iostream>
#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>

#include <onnxruntime_cxx_api.h>

#include "json.hpp"
#include "speaker.hpp"

namespace speaker
{

#ifdef SPEAKER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
    const std::string VERSION = STR(SPEAKER_VERSION);
#else
    const std::string VERSION = "";
#endif

    speaker::Model model;

    std::string getVersion()
    {
        return VERSION;
    }

    void parseModelConfig(json &configRoot, ModelConfig &modelConfig)
    {
        if (configRoot.contains("data"))
        {
            auto dataValue = configRoot["data"];
            if (dataValue.contains("max_wav_value"))
            {
                modelConfig.maxWavValue = dataValue.value("max_wav_value", 32767.0f);
            }
            if (dataValue.contains("sample_rate"))
            {
                modelConfig.sampleRate = dataValue.value("sample_rate", 16000);
            }
            if (dataValue.contains("noise_scale"))
            {
                modelConfig.noiseScale = dataValue.value("noise_scale", 0.667f);
            }
            if (dataValue.contains("length_scale"))
            {
                modelConfig.lengthScale = dataValue.value("length_scale", 1.0f);
            }
            if (dataValue.contains("noise_w"))
            {
                modelConfig.noiseW = dataValue.value("noise_w", 0.8f);
            }
            if (dataValue.contains("n_speakers"))
            {
                modelConfig.numSpeakers = dataValue.value("n_speakers", 1);
            }
        }
    }

    void loadModel(const std::string &modelPath, const std::string &modelConfigPath, int16_t numThreads)
    {
        std::ifstream modelConfigFile(modelConfigPath);
        json configRoot = json::parse(modelConfigFile);
        parseModelConfig(configRoot, model.config);
        model.session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "speaker");
        model.session.env.DisableTelemetryEvents();
        if (numThreads > 0)
        {
            model.session.options.SetIntraOpNumThreads(numThreads);
        }
        model.session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        model.session.options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        model.session.options.DisableCpuMemArena();
        model.session.options.DisableMemPattern();
        model.session.options.DisableProfiling();

        model.session.session = Ort::Session(model.session.env, modelPath.c_str(), model.session.options);
    }

    void synthesize(std::vector<int64_t> &phonemeIds, float speechRate, std::vector<int16_t> &audioBuffer, SynthesisResult &result)
    {
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        std::vector<int64_t> phonemeIdsLength{(int64_t)phonemeIds.size()};
        std::vector<float> scales{
            model.config.noiseScale,
            model.config.lengthScale / speechRate,
            model.config.noiseW};
        std::vector<int64_t> speakerId{(int64_t)model.config.speakerId};

        std::vector<Ort::Value> inputTensors;
        std::vector<int64_t> phonemeIdsShape{1, (int64_t)phonemeIds.size()};
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            phonemeIds.data(),
            phonemeIds.size(),
            phonemeIdsShape.data(),
            phonemeIdsShape.size()));

        std::vector<int64_t> phomemeIdsLengthShape{(int64_t)phonemeIdsLength.size()};
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            phonemeIdsLength.data(),
            phonemeIdsLength.size(),
            phomemeIdsLengthShape.data(),
            phomemeIdsLengthShape.size()));

        std::vector<int64_t> scalesShape{(int64_t)scales.size()};
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo,
            scales.data(),
            scales.size(),
            scalesShape.data(),
            scalesShape.size()));

        std::vector<int64_t> speakerIdShape{(int64_t)speakerId.size()};
        if (model.config.speakerId)
        {
            inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            speakerId.data(),
            speakerId.size(),
            speakerIdShape.data(),
            speakerIdShape.size()));
        }

        std::array<const char *, 4> inputNames = {"input", "input_lengths", "scales", "sid"};
        std::array<const char *, 1> outputNames = {"output"};

        auto startTime = std::chrono::steady_clock::now();
        auto outputTensors = model.session.session.Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            inputTensors.data(),
            inputTensors.size(),
            outputNames.data(),
            outputNames.size());
        auto endTime = std::chrono::steady_clock::now();

        if ((outputTensors.size() != 1) || (!outputTensors.front().IsTensor()))
        {
            throw std::runtime_error("invalid output tensors");
        }

        auto inferDuration = std::chrono::duration<double>(endTime - startTime);
        result.inferDuration = inferDuration.count() * 1000;

        const float *audio = outputTensors.front().GetTensorData<float>();
        auto audioShape = outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
        int64_t audioSamples = audioShape[audioShape.size() - 1];
        result.audioDuration = ((double)audioSamples / (double)model.config.sampleRate) * 1000;

        result.realTimeFactor = 0.0;
        if (result.audioDuration > 0)
        {
            result.realTimeFactor = result.inferDuration / result.audioDuration;
        }

        float maxAudioValue = 0.01f;
        for (int64_t i = 0; i < audioSamples; i++)
        {
            float audioValue = abs(audio[i]);
            if (audioValue > maxAudioValue)
            {
                maxAudioValue = audioValue;
            }
        }

        audioBuffer.reserve(audioSamples);

        float audioValueScale = (model.config.maxWavValue / std::max(0.01f, maxAudioValue));
        for (int64_t i = 0; i < audioSamples; i++)
        {
            int16_t intAudioValue = static_cast<int16_t>(
                std::clamp(audio[i] * audioValueScale,
                           static_cast<float>(std::numeric_limits<int16_t>::min()),
                           static_cast<float>(std::numeric_limits<int16_t>::max())));

            audioBuffer.push_back(intAudioValue);
        }
        
        for (std::size_t i = 0; i < outputTensors.size(); i++)
        {
            Ort::detail::OrtRelease(outputTensors[i].release());
        }

        for (std::size_t i = 0; i < inputTensors.size(); i++)
        {
            Ort::detail::OrtRelease(inputTensors[i].release());
        }
    }

}