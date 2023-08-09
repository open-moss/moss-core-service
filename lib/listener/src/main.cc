#include <iomanip>
#include <utility>
#include <thread>
#include <queue>

#include "decoder/params.h"
#include "utils/flags.h"
#include "utils/string.h"
#include "utils/timer.h"
#include "utils/utils.h"
#include "vad/vad.h"

#include "json.hpp"

#ifdef _LISTENER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
const std::string VERSION = STR(_LISTENER_VERSION);
#else
const std::string VERSION = "";
#endif

using json = nlohmann::json;

struct DecodeResult {
  std::string final_result;
  int decode_duration;
  int audio_duration;
};

struct SampledData {
  std::vector<float> data;
  float start_time;
  float end_time;
};

std::shared_ptr<wenet::DecodeOptions> g_decode_config;
std::shared_ptr<wenet::FeaturePipelineConfig> g_feature_config;
std::shared_ptr<wenet::DecodeResource> g_decode_resource;

std::queue<SampledData> decodeQueue;
std::mutex mtx;
std::condition_variable cv;

void Decode(std::vector<char> &data, DecodeResult &result) {
  int num_samples = data.size() / 2;

  auto feature_pipeline =
      std::make_shared<wenet::FeaturePipeline>(*g_feature_config);

  feature_pipeline->AcceptWaveform(reinterpret_cast<const int16_t*>(data.data()), num_samples);
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

void ProcessDecode() {
  while (true) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ return !decodeQueue.empty(); });
    if (decodeQueue.empty()) {
      continue;
    }
    SampledData sampled_data = decodeQueue.front();
    decodeQueue.pop();
    std::vector<char> decode_data(sampled_data.data.size() * 2);
    for (int i = 0; i < sampled_data.data.size(); i++) {
      int16_t int16Value = static_cast<int16_t>(sampled_data.data[i] * 32767); // 乘以32767，范围映射到[-32767, 32767]
      decode_data.push_back(static_cast<char>(int16Value & 0xFF));
      decode_data.push_back(static_cast<char>((int16Value >> 8) & 0xFF));
    }
    DecodeResult decode_result;
    Decode(decode_data, decode_result);
    
    json result = {
      { "start_time", static_cast<int>(round(sampled_data.start_time * 1000)) },
      { "end_time", static_cast<int>(round(sampled_data.end_time * 1000)) },
      { "result", decode_result.final_result },
      { "decode_duration", decode_result.decode_duration },
      { "audio_duration", decode_result.audio_duration },
      { "rtf", round((static_cast<float>(decode_result.decode_duration) / decode_result.audio_duration) * 1000.0) / 1000.0 }
    };
    std::cout << result.dump() << std::endl;

    lock.unlock();
  }
}

int main(int argc, char* argv[]) {
  gflags::SetVersionString(VERSION);
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  g_decode_config = wenet::InitDecodeOptionsFromFlags();
  g_feature_config = wenet::InitFeaturePipelineConfigFromFlags();
  g_decode_resource = wenet::InitDecodeResourceFromFlags();

  VadIterator vad(FLAGS_onnx_dir + "/vad.onnx", FLAGS_sample_rate, 64, 0.6f, 0, 0);
  
  std::ifstream file;
  file.open("out.pcm",  std::ios::binary);
  file.seekg(0, std::ios::end);
  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(fileSize);
  file.read(buffer.data(), fileSize);
  file.close();

  int window_samples = 64 * (FLAGS_sample_rate / 1000);
  int num_samples = fileSize / 2;

  std::vector<float> input_data(num_samples);
  for (int i = 0; i < num_samples; ++i) {
    char lowByte = buffer[i * 2];
    char highByte = buffer[i * 2 + 1];
    int16_t int16Value = static_cast<int16_t>(lowByte) | (static_cast<int16_t>(highByte) << 8);
    input_data[i] = static_cast<float>(int16Value) / 32767.0f;
  }

  std::thread decode_thread(ProcessDecode);

  std::vector<float> data;
  float last_start_time = 0;
  bool is_sampling = false;
  for (int j = 0; j < num_samples; j += window_samples)
  {
      std::vector<float> r{&input_data[0] + j, &input_data[0] + j + window_samples};
      vad.Predict(r, [&last_start_time, &data, &is_sampling](float start_time) {
        data.clear();
        is_sampling = true;
        last_start_time = start_time;
      }, [&last_start_time, &data, &is_sampling](float end_time) {
        decodeQueue.push({ data, last_start_time, end_time });
        data.clear();
        cv.notify_one();
        is_sampling = false;
      });
      if(is_sampling) {
        data.insert(data.end(), r.begin(), r.end());
      }
  }

  decode_thread.join();

  return 0;
}
