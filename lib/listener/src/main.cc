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

DEFINE_int32(vad_window_frame_size, 64, "vad iterator window frame size");
DEFINE_double(vad_threshold, 0.6f, "vad iterator threshold");

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
  int64_t start_time;
  int64_t end_time;
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

std::string build_message(int code, std::string message, json others) {
  json obj = {
    { "code", code },
    { "message", message },
    { "data", others }
  };
  return obj.dump();
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
      int16_t int16Value = static_cast<int16_t>(sampled_data.data[i] * 32767);
      decode_data.push_back(static_cast<char>(int16Value & 0xFF));
      decode_data.push_back(static_cast<char>((int16Value >> 8) & 0xFF));
    }
    DecodeResult decode_result;
    Decode(decode_data, decode_result);

    std::cout << build_message(0, "OK", {
      { "event", "decode_end" },
      { "start_time", sampled_data.start_time },
      { "end_time", sampled_data.end_time },
      { "result", decode_result.final_result },
      { "decode_duration", decode_result.decode_duration },
      { "audio_duration", decode_result.audio_duration },
      { "rtf", round((static_cast<float>(decode_result.decode_duration) / decode_result.audio_duration) * 1000.0) / 1000.0 }
    }) << std::endl;

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

  VadIterator vad(FLAGS_onnx_dir + "/vad.onnx", FLAGS_sample_rate, FLAGS_vad_window_frame_size, FLAGS_vad_threshold, 0, 0);

  std::thread decode_thread(ProcessDecode);

  std::cout << build_message(0, "OK", {
    {"event", "wait_input"}
  }) << std::endl;

  int window_samples = FLAGS_vad_window_frame_size * (FLAGS_sample_rate / 1000);
  int total_samples = 0;
  int total_size = 0;
  string line;
  std::vector<char> temp;
  std::vector<float> data;
  int64_t last_start_time = 0;
  bool is_sampling = false;
  while (getline(std::cin, line)) {
    std::vector<char> buffer(line.begin(), line.end());
    buffer.push_back(0x0A);

    if(temp.size() > 0) {
      buffer.insert(buffer.begin(), temp.begin(), temp.end());
      temp.clear();
    }
    int window_samples_size = window_samples * 2;
    if (buffer.size() < window_samples_size) {
      temp.insert(temp.end(), buffer.begin(), buffer.end());
      continue;
    }
    else {
      int remain = buffer.size() % window_samples_size;
      if(remain > 0) {
        temp.insert(temp.end(), buffer.end() - remain, buffer.end());
        buffer.resize(buffer.size() - remain);
      }
    }

    int num_samples = buffer.size() / 2;

    std::vector<float> input_data(num_samples);
    for (int i = 0; i < num_samples; i++) {
      char lowByte = buffer[i * 2];
      char highByte = buffer[i * 2 + 1];
      int16_t int16Value = static_cast<int16_t>(lowByte) | (static_cast<int16_t>(highByte) << 8);
      input_data[i] = static_cast<float>(int16Value) / 32767.0f;
    }
    
    for (int j = 0; j < num_samples; j += window_samples)
    {
        std::vector<float> r{&input_data[0] + j, &input_data[0] + j + window_samples};
        vad.Predict(r, [&last_start_time, &data, &is_sampling](int start_time) {
          data.clear();
          is_sampling = true;
          last_start_time = start_time;
        }, [&last_start_time, &data, &is_sampling](int end_time) {
          decodeQueue.push({ data, last_start_time, end_time });
          cv.notify_one();
          data.clear();
          is_sampling = false;
        });
        if(is_sampling) {
          data.insert(data.end(), r.begin(), r.end());
        }
    }

  }

  decode_thread.join();

  return 0;
}