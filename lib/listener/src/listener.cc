#include <iomanip>
#include <utility>

#include "decoder/params.h"
#include "utils/flags.h"
#include "utils/string.h"
#include "utils/timer.h"
#include "utils/utils.h"
#include "json.hpp"

using json = nlohmann::json;

std::shared_ptr<wenet::DecodeOptions> g_decode_config;
std::shared_ptr<wenet::FeaturePipelineConfig> g_feature_config;
std::shared_ptr<wenet::DecodeResource> g_decode_resource;

std::ofstream g_result;
std::mutex g_mutex;
bool g_buffer_receiving = false;

void Decode(std::vector<char> &buffer) {
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

  std::cout << "{\"code\":0,\"message\":\"OK\",\"text\":\""
  << final_result
  << "\",\"decode_duration\":"
  << decode_duration
  << ",\"audio_duration\":"
  << audio_duration
  << ",\"rtf\":"
  << std::setprecision(4)
  << static_cast<float>(decode_duration) / audio_duration
  << "}" << std::endl;

}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  g_decode_config = wenet::InitDecodeOptionsFromFlags();
  g_feature_config = wenet::InitFeaturePipelineConfigFromFlags();
  g_decode_resource = wenet::InitDecodeResourceFromFlags();

  std::vector<char> buffer;
  std::string line;

  std::cout << "{\"code\":0,\"message\":\"waiting for input data...\"}" << std::endl;
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
      std::cout << "{\"code\":-1,\"message\":\"event must be provided\"}" << std::endl;
      continue;
    }
    auto event = lineRoot["event"].get<std::string>();
    if(event == "buffer_start") {
      buffer.clear();
      g_buffer_receiving = true;
      std::cout << "{\"code\":0,\"message\":\"OK\"}" << std::endl;
    }
    else if(event == "decode_start") {
      g_buffer_receiving = false;
      buffer.pop_back();
      Decode(buffer);
      buffer.clear();
    }
  }

  return 0;
}
