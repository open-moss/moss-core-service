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

namespace speaker {

typedef int64_t SpeakerId;

struct ModelConfig {
  float noiseScale = 0.667f;
  float lengthScale = 1.0f;
  float noiseW = 0.8f;
  int numSpeakers = 1;
  int sampleRate = 16000;
  int sampleWidth = 2; // 16-bit
  int channels = 1;    // mono
  std::optional<SpeakerId> speakerId;
  float sentenceSilenceSeconds = 0.2f;
};

struct ModelSession {
  Ort::Session onnx;
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::SessionOptions options;
  Ort::Env env;

  ModelSession() : onnx(nullptr){};
};

struct SynthesisResult {
  bool success = false;
  double inferSeconds;
  double audioSeconds;
  double realTimeFactor;
};

struct Model {
  json configRoot;
  ModelConfig config;
  ModelSession session;
};

// Get version of Speaker
std::string getVersion();

// Load Onnx model and JSON config file
void loadModel(std::string modelPath,
               std::string modelConfigPath,
               int16_t numThreads,
               Model &model,
               std::optional<SpeakerId> &speakerId);


// synthesize audio
void synthesize(std::vector<int64_t> &phonemeIds, float speechRate,
                ModelConfig &modelConfig, ModelSession &session,
                std::vector<int16_t> &audioBuffer, SynthesisResult &result);

} // namespace speaker

#endif // SPEAKER_H_
