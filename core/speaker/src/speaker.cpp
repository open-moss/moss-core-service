#include <iostream>
#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>

#include <onnxruntime_cxx_api.h>
#include <alsa/asoundlib.h>

#include "speaker.hpp"

namespace speaker
{

    speaker::Model model;
    snd_pcm_t *pcmHandle;
    std::string audioDeviceName;
    std::string audioMixerName;

#ifdef USE_ALSA
    void alsaOpen()
    {
        snd_pcm_hw_params_t *params;
        snd_pcm_open(&pcmHandle, audioDeviceName.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        snd_pcm_hw_params_malloc(&params);
        snd_pcm_hw_params_any(pcmHandle, params);
        snd_pcm_hw_params_set_access(pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcmHandle, params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(pcmHandle, params, 1);
        snd_pcm_hw_params_set_rate_near(pcmHandle, params, &model.config.sampleRate, NULL);
        snd_pcm_hw_params(pcmHandle, params);
        snd_pcm_hw_params_free(params);
        snd_pcm_prepare(pcmHandle);
    }

    void alsaSetVolume(const uint16_t &volume)
    {
        snd_mixer_t *mixerHandle;
        snd_mixer_open(&mixerHandle, 0);
        snd_mixer_attach(mixerHandle, audioDeviceName.c_str());
        snd_mixer_selem_register(mixerHandle, NULL, NULL);
        snd_mixer_load(mixerHandle);
        snd_mixer_selem_id_t *sid;
        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, audioMixerName.c_str());
        snd_mixer_elem_t* elem = snd_mixer_find_selem(mixerHandle, sid);
        if (elem == NULL)
        {
            throw std::runtime_error("audio controller not found: ");
        }
        long min, max, _volume;
        // 获取音量范围
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        // 计算新的音量值（基于百分比）
        _volume = (max - min) * ((double)volume / 100) + min;
        // 设置新音量值
        snd_mixer_selem_set_playback_volume_all(elem, _volume);
        // 关闭混音器
        snd_mixer_close(mixerHandle);
    }
#endif
    
    void initialize(const std::string &modelPath, const ModelConfig &modelConfig, const uint16_t &numThreads, const std::string &_audioDeviceName, const std::string &_audioMixerName)
    {
        model.config = modelConfig;
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
        audioDeviceName = std::move(_audioDeviceName);
        audioMixerName = std::move(_audioMixerName);
#ifdef USE_ALSA
        alsaOpen();
#endif
    }

    void setVolume(const uint16_t &volume)
    {
#ifdef USE_ALSA
        alsaSetVolume(volume);
#else
        throw std::runtime_error("please USE_ALSA!");
#endif
    }

    void synthesize(std::vector<int64_t> &phonemeIds, const uint16_t &speakerId, const float &speechRate, std::vector<int16_t> &audioBuffer, SynthesisResult &result)
    {
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        std::vector<int64_t> phonemeIdsLength{(int64_t)phonemeIds.size()};
        std::vector<float> scales{
            model.config.noiseScale,
            model.config.lengthScale / speechRate,
            model.config.noiseW};
        std::vector<int64_t> speakerIdVector{ speakerId };

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

        if (!model.config.singleSpeaker)
        {
            std::vector<int64_t> speakerIdShape{(int64_t)speakerIdVector.size()};
            inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
                memoryInfo,
                speakerIdVector.data(),
                speakerIdVector.size(),
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

    void say(std::vector<int64_t> &phonemeIds, const uint16_t &speakerId, const float &speechRate, const bool &block, SynthesisResult &result) {
#ifdef USE_ALSA
        snd_pcm_state_t pcmHandleState = snd_pcm_state(pcmHandle);
        std::cout << pcmHandleState << std::endl;
        switch(pcmHandleState) {
            case SND_PCM_STATE_SETUP:
                snd_pcm_prepare(pcmHandle);
            break;
            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_DRAINING:
                snd_pcm_drop(pcmHandle);
                snd_pcm_reset(pcmHandle);
                snd_pcm_prepare(pcmHandle);
            break;
            case SND_PCM_STATE_XRUN:
            case SND_PCM_STATE_PAUSED:
                snd_pcm_reset(pcmHandle);
                snd_pcm_prepare(pcmHandle);
        }
        std::vector<int16_t> audioBuffer;
        synthesize(phonemeIds, speakerId, speechRate, audioBuffer, result);
        snd_pcm_writei(pcmHandle, audioBuffer.data(), audioBuffer.size());
        if(block)
            snd_pcm_drain(pcmHandle);
#else
        throw std::runtime_error("please USE_ALSA!");
#endif
    }

}