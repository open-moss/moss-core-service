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
// {"phoneme_ids":[0,47,0,11,0,28,0,47,0,49,0,21,0,38,0,47,0,32,0,47,0,1,0,49,0,26,0,42,0,39,0,45,0,49,0,7,0,42,0,45,0,23,0,48,0,49,0,24,0,42,0,29,0,47,0,20,0,41,0,34,0,46,0,1,0,49,0,40,0,45,0,23,0,47,0,49,0,34,0,47,0,9,0,36,0,47,0,2,0]}
// Get version of Speaker
std::string getVersion();

// Load Onnx model and JSON config file
void loadModel(std::string modelPath,
               std::string modelConfigPath, Model &model,
               std::optional<SpeakerId> &speakerId);


// synthesize audio
void synthesize(std::vector<int64_t> &phonemeIds, float speechRate,
                ModelConfig &modelConfig, ModelSession &session,
                std::vector<int16_t> &audioBuffer, SynthesisResult &result);

// synthesize audio to WAV file
void synthesizeToWavFile(Model &model, std::vector<int64_t> phonemeIds, float speechRate,
                   std::ostream &audioFile, SynthesisResult &result);

} // namespace speaker

#endif // SPEAKER_H_
