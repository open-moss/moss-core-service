#include <iomanip>
#include <utility>

#include "decoder/params.h"
#include "utils/flags.h"
#include "utils/string.h"
#include "utils/timer.h"
#include "utils/utils.h"
#include "json.hpp"

#ifdef _LISTENER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
const std::string VERSION = STR(_LISTENER_VERSION);
#else
const std::string VERSION = "";
#endif

using json = nlohmann::json;

std::shared_ptr<wenet::DecodeOptions> g_decode_config;
std::shared_ptr<wenet::FeaturePipelineConfig> g_feature_config;
std::shared_ptr<wenet::DecodeResource> g_decode_resource;

std::ofstream g_result;
std::mutex g_mutex;
bool g_buffer_receiving = false;

struct DecodeResult {
  std::string final_result;
  int decode_duration;
  int audio_duration;
};

void Decode(std::vector<char> &buffer, DecodeResult &result) {
  int num_samples = buffer.size() / 2;

  auto feature_pipeline =
      std::make_shared<wenet::FeaturePipeline>(*g_feature_config);

  feature_pipeline->AcceptWaveform(reinterpret_cast<const int16_t*>(buffer.data()), num_samples);
  feature_pipeline->set_input_finished();

  wenet::AsrDecoder decoder(feature_pipeline, g_decode_resource,
                            *g_decode_config);

  int audio_duration = static_cast<int>(static_cast<float>(num_samples) /
                                  FLAGS_sample_rate * 1000);
  int decode_duration = 0;
  std::string final_result;
  while (true) {
    wenet::Timer timer;
    wenet::DecodeState state = decoder.Decode();
    if (state == wenet::DecodeState::kEndFeats) {
      decoder.Rescoring();
    }
    int chunk_decode_time = timer.Elapsed();
    decode_duration += chunk_decode_time;
    // if (decoder.DecodedSomething()) {
    //   std::cout << "Partial result: " << decoder.result()[0].sentence << std::endl;
    // }

    if (state == wenet::DecodeState::kEndFeats) {
      break;
    }
  }
  if (decoder.DecodedSomething()) {
    final_result.append(decoder.result()[0].sentence);
  }
  result.final_result = final_result;
  result.audio_duration = audio_duration;
  result.decode_duration = decode_duration;
}

std::string build_message(int code, std::string message, json others) {
  json obj = {
    { "code", code },
    { "message", message },
    { "data", others }
  };
  return obj.dump();
}

int main(int argc, char* argv[]) {
  gflags::SetVersionString(VERSION);
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  g_decode_config = wenet::InitDecodeOptionsFromFlags();
  g_feature_config = wenet::InitFeaturePipelineConfigFromFlags();
  g_decode_resource = wenet::InitDecodeResourceFromFlags();

  std::vector<char> buffer;
  std::string line;
  DecodeResult result;

  std::cout << build_message(0, "OK", {
    {"event", "wait_input"}
  }) << std::endl;

  while (getline(std::cin, line)) {
    json lineRoot = NULL;
    if (line.length() > 0 && line.at(0) == '{' && line.at(line.length() - 1) == '}') {
      try {
        lineRoot = json::parse(line);
      }
      catch(const std::exception& e) {
        lineRoot = NULL;
      }
    }
    if (lineRoot == NULL) {
      if(!g_buffer_receiving) {
        continue;
      }
      line += 0x0A;
      buffer.insert(buffer.end(), line.begin(), line.end());
      continue;
    }
    if(!lineRoot.contains("event")) {
      std::cout << build_message(-1, "event must be provided", NULL) << std::endl;
      continue;
    }
    auto event = lineRoot["event"].get<std::string>();
    if(event == "buffer_start") {
      buffer.clear();
      g_buffer_receiving = true;
      std::cout << build_message(0, "OK", {
        { "event", "buffer_start" }
      }) << std::endl;
    }
    else if(event == "decode_start") {
      g_buffer_receiving = false;
      buffer.pop_back();
      Decode(buffer, result);
      buffer.clear();
      std::cout << build_message(0, "OK", {
        { "event", "decode_end" },
        { "result", result.final_result },
        { "decode_duration", result.decode_duration },
        { "audio_duration", result.audio_duration },
        { "rtf", round((static_cast<float>( result.decode_duration) / result.audio_duration) * 1000.0) / 1000.0 }
      }) << std::endl;
    }
  }

  return 0;
}
