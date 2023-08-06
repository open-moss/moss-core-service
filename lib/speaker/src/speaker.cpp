#include <iostream>
#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <onnxruntime_cxx_api.h>
#include <spdlog/spdlog.h>

#include "json.hpp"
#include "speaker.hpp"
#include "utf8.h"

namespace speaker {

#ifdef _SPEAKER_VERSION
// https://stackoverflow.com/questions/47346133/how-to-use-a-define-inside-a-format-string
#define _STR(x) #x
#define STR(x) _STR(x)
const std::string VERSION = STR(_SPEAKER_VERSION);
#else
const std::string VERSION = "";
#endif

// Maximum value for 16-bit signed WAV sample
const float MAX_WAV_VALUE = 32767.0f;

const std::string instanceName{"speaker"};

std::string getVersion() {
  return VERSION;
}

// True if the string is a single UTF-8 codepoint
bool isSingleCodepoint(std::string s) {
  return utf8::distance(s.begin(), s.end()) == 1;
}

void parseModelConfig(json &configRoot, ModelConfig &modelConfig) {

  if (configRoot.contains("data")) {
    auto dataValue = configRoot["data"];
    if (dataValue.contains("sample_rate")) {
      // Default sample rate is 16000 Hz
      modelConfig.sampleRate = dataValue.value("sample_rate", 16000);
    }
    if (dataValue.contains("noise_scale")) {
      modelConfig.noiseScale = dataValue.value("noise_scale", 0.667f);
    }
    if (dataValue.contains("length_scale")) {
      modelConfig.lengthScale = dataValue.value("length_scale", 1.0f);
    }
    if (dataValue.contains("noise_w")) {
      modelConfig.noiseW = dataValue.value("noise_w", 0.8f);
    }
    if (dataValue.contains("n_speakers")) {
      modelConfig.numSpeakers = dataValue["n_speakers"].get<SpeakerId>();
    }
  }

} /* parseModelConfig */

// Load Onnx model and JSON config file
void loadModel(std::string modelPath,
               std::string modelConfigPath,
               int16_t numThreads,
               Model &model,
               std::optional<SpeakerId> &speakerId) {
  spdlog::debug("Parsing model config at {}", modelConfigPath);
  std::ifstream modelConfigFile(modelConfigPath);
  model.configRoot = json::parse(modelConfigFile);

  parseModelConfig(model.configRoot, model.config);

  if (model.config.numSpeakers > 1) {
    // Multi-speaker model
    if (speakerId) {
      model.config.speakerId = speakerId;
    } else {
      // Default speaker
      model.config.speakerId = 0;
    }
  }

  spdlog::debug("Model contains {} speaker(s)", model.config.numSpeakers);

  spdlog::debug("Loading onnx model from {}", modelPath);
  model.session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                         instanceName.c_str());
  model.session.env.DisableTelemetryEvents();

  // Slows down performance by ~2x
  if(numThreads > 0) {
    model.session.options.SetIntraOpNumThreads(numThreads);
  }

  // Roughly doubles load time for no visible inference benefit
  // model.session.options.SetGraphOptimizationLevel(
  //     GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

  model.session.options.SetGraphOptimizationLevel(
      GraphOptimizationLevel::ORT_ENABLE_ALL);

  model.session.options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

  model.session.options.DisableCpuMemArena();
  model.session.options.DisableMemPattern();
  model.session.options.DisableProfiling();

  auto startTime = std::chrono::steady_clock::now();
  model.session.onnx = Ort::Session(model.session.env, modelPath.c_str(), model.session.options);
  auto endTime = std::chrono::steady_clock::now();
  spdlog::debug("Loaded onnx model in {} second(s)",
                std::chrono::duration<double>(endTime - startTime).count());

} /* loadModel */

// Phoneme ids to WAV audio
void synthesize(std::vector<int64_t> &phonemeIds, float speechRate,
                ModelConfig &modelConfig, ModelSession &session,
                std::vector<int16_t> &audioBuffer, SynthesisResult &result) {
  spdlog::debug("Synthesizing audio for {} phoneme id(s)", phonemeIds.size());

  try
  {
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(
      OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    // Allocate
    std::vector<int64_t> phonemeIdLengths{(int64_t)phonemeIds.size()};
    std::vector<float> scales{modelConfig.noiseScale,
                              modelConfig.lengthScale / speechRate,
                              modelConfig.noiseW};

    std::vector<Ort::Value> inputTensors;
    std::vector<int64_t> phonemeIdsShape{1, (int64_t)phonemeIds.size()};
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, phonemeIds.data(), phonemeIds.size(), phonemeIdsShape.data(),
        phonemeIdsShape.size()));

    std::vector<int64_t> phomemeIdLengthsShape{(int64_t)phonemeIdLengths.size()};
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, phonemeIdLengths.data(), phonemeIdLengths.size(),
        phomemeIdLengthsShape.data(), phomemeIdLengthsShape.size()));

    std::vector<int64_t> scalesShape{(int64_t)scales.size()};
    inputTensors.push_back(
        Ort::Value::CreateTensor<float>(memoryInfo, scales.data(), scales.size(),
                                        scalesShape.data(), scalesShape.size()));

    // Add speaker id.
    // NOTE: These must be kept outside the "if" below to avoid being deallocated.
    std::vector<int64_t> speakerId{
        (int64_t)modelConfig.speakerId.value_or(0)};
    std::vector<int64_t> speakerIdShape{(int64_t)speakerId.size()};

    if (modelConfig.speakerId) {
      inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memoryInfo, speakerId.data(), speakerId.size(), speakerIdShape.data(),
          speakerIdShape.size()));
    }

    // From export_onnx.py
    std::array<const char *, 4> inputNames = {"input", "input_lengths", "scales",
                                          "sid" };
    std::array<const char *, 1> outputNames = {"output"};

    // Infer
    auto startTime = std::chrono::steady_clock::now();
    auto outputTensors = session.onnx.Run(
        Ort::RunOptions{nullptr}, inputNames.data(), inputTensors.data(),
        inputTensors.size(), outputNames.data(), outputNames.size());
    auto endTime = std::chrono::steady_clock::now();

    if ((outputTensors.size() != 1) || (!outputTensors.front().IsTensor())) {
      std::cout << "{\"code\":-4,\"message\":\"Invalid output tensors\"" << std::endl;
      return;
    }
    auto inferDuration = std::chrono::duration<double>(endTime - startTime);
    result.inferSeconds = inferDuration.count();

    const float *audio = outputTensors.front().GetTensorData<float>();
    auto audioShape =
        outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
    int64_t audioCount = audioShape[audioShape.size() - 1];

    result.audioSeconds = (double)audioCount / (double)modelConfig.sampleRate;
    result.realTimeFactor = 0.0;
    if (result.audioSeconds > 0) {
      result.realTimeFactor = result.inferSeconds / result.audioSeconds;
    }
    spdlog::debug("Synthesized {} second(s) of audio in {} second(s)",
                  result.audioSeconds, result.inferSeconds);

    // Get max audio value for scaling
    float maxAudioValue = 0.01f;
    for (int64_t i = 0; i < audioCount; i++) {
      float audioValue = abs(audio[i]);
      if (audioValue > maxAudioValue) {
        maxAudioValue = audioValue;
      }
    }

    // We know the size up front
    audioBuffer.reserve(audioCount);

    // Scale audio to fill range and convert to int16
    float audioScale = (MAX_WAV_VALUE / std::max(0.01f, maxAudioValue));
    for (int64_t i = 0; i < audioCount; i++) {
      int16_t intAudioValue = static_cast<int16_t>(
          std::clamp(audio[i] * audioScale,
                    static_cast<float>(std::numeric_limits<int16_t>::min()),
                    static_cast<float>(std::numeric_limits<int16_t>::max())));

      audioBuffer.push_back(intAudioValue);
    }

    // Clean up
    for (std::size_t i = 0; i < outputTensors.size(); i++) {
      Ort::detail::OrtRelease(outputTensors[i].release());
    }

    for (std::size_t i = 0; i < inputTensors.size(); i++) {
      Ort::detail::OrtRelease(inputTensors[i].release());
    }

    result.success = true;
  }
  catch(const std::exception& e)
  {
    std::cout << "{\"code\":-3,\"message\":\"" << e.what() << "\"}" << std::endl;
    result.success = false;
  }
  
}

} // namespace speaker
