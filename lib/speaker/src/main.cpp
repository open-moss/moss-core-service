#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "json.hpp"
#include "speaker.hpp"

using namespace std;
using json = nlohmann::json;

enum OutputType { OUTPUT_DIRECTORY, OUTPUT_RAW };

struct RunConfig {
  // Path to .onnx model file
  filesystem::path modelPath;

  // Path to JSON model config file
  filesystem::path modelConfigPath;

  // Type of output to produce.
  // Default is to write a WAV file in the current directory.
  OutputType outputType = OUTPUT_RAW;

  // Path for output
  optional<filesystem::path> outputPath = filesystem::path(".");

  // Numerical id of the default speaker (multi-speaker voices)
  optional<speaker::SpeakerId> speakerId;

  // Amount of noise to add during audio generation
  optional<float> noiseScale;

  // Speed of speaking (1 = normal, < 1 is faster, > 1 is slower)
  optional<float> lengthScale;

  // Variation in phoneme lengths
  optional<float> noiseW;

  // Seconds of silence to add after each sentence
  optional<float> sentenceSilenceSeconds;

  // stdin input is lines of JSON instead of text with format:
  // {
  //   "text": str,               (required)
  //   "speaker_id": int,         (optional)
  //   "speaker": str,            (optional)
  //   "output_file": str,        (optional)
  // }
};

void parseArgs(int argc, char *argv[], RunConfig &runConfig);
void rawOutputProc(vector<int16_t> &sharedAudioBuffer, mutex &mutAudio,
                   condition_variable &cvAudio, bool &audioReady,
                   bool &audioFinished);

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  spdlog::set_default_logger(spdlog::stderr_color_st("speaker"));

  RunConfig runConfig;
  parseArgs(argc, argv, runConfig);

  speaker::Model model;

  spdlog::debug("Loading model from {} (config={})",
                runConfig.modelPath.string(),
                runConfig.modelConfigPath.string());

  auto startTime = chrono::steady_clock::now();
  loadModel(runConfig.modelPath.string(),
            runConfig.modelConfigPath.string(), model, runConfig.speakerId);
  auto endTime = chrono::steady_clock::now();
  spdlog::info("Loaded model in {} second(s)",
               chrono::duration<double>(endTime - startTime).count());

#ifdef _MSC_VER
  auto exePath = []() {
    wchar_t moduleFileName[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, moduleFileName, std::size(moduleFileName));
    return filesystem::path(moduleFileName);
  }();
#elifdef __APPLE__
  auto exePath = []() {
    char moduleFileName[PATH_MAX] = {0};
    uint32_t moduleFileNameSize = std::size(moduleFileName);
    _NSGetExecutablePath(moduleFileName, &moduleFileNameSize);
    return filesystem::path(moduleFileName);
  }();
#else
  auto exePath = filesystem::canonical("/proc/self/exe");
#endif

  // Scales
  if (runConfig.noiseScale) {
    model.config.noiseScale = runConfig.noiseScale.value();
  }

  if (runConfig.lengthScale) {
    model.config.lengthScale = runConfig.lengthScale.value();
  }

  if (runConfig.noiseW) {
    model.config.noiseW = runConfig.noiseW.value();
  }

  if (runConfig.sentenceSilenceSeconds) {
    model.config.sentenceSilenceSeconds = runConfig.sentenceSilenceSeconds.value();
  }

  if (runConfig.outputType == OUTPUT_DIRECTORY) {
    runConfig.outputPath = filesystem::absolute(runConfig.outputPath.value());
    spdlog::info("Output directory: {}", runConfig.outputPath.value().string());
  }

  cout << "{\"code\":0,\"message\":\"waiting for input data...\"}" << endl;
  
  string line;
  speaker::SynthesisResult result;
  while (getline(cin, line)) {
    auto outputType = runConfig.outputType;
    auto speakerId = model.config.speakerId;
    
    std::optional<filesystem::path> maybeOutputPath = runConfig.outputPath;
  
    json lineRoot;
    try
    {
      lineRoot = json::parse(line);
    }
    catch(const std::exception& e)
    {
      cout << "{\"code\":-1,\"message\":\"input data must be an json object\"}" << endl;
      continue;
    }
    
    if (!lineRoot.is_object())
    {
      cout << "{\"code\":-1,\"message\":\"input data must be an json object\"}" << endl;
      continue;
    }

    uint16_t taskId = 0;
    std::vector<int64_t> phonemeIds;
    float speechRate = 1.0f;
    try
    {
      if (lineRoot.contains("task_id")) {
        taskId = lineRoot["task_id"].get<uint16_t>();
      }

      // phonemeIds is required
      if (lineRoot.contains("phoneme_ids")) {
        auto phonemeIdsValue = lineRoot["phoneme_ids"];
        for(auto &phonemeIdItem : phonemeIdsValue.items())
        {
          phonemeIds.push_back(phonemeIdItem.value());
        }
      }
      else {
        cout << "{\"code\":-2,\"message\":\"phoneme_ids must be provided\"}" << endl;
        continue;
      }

      if (lineRoot.contains("speaker_id")) {
        // Override speaker id
        model.config.speakerId = lineRoot["speaker_id"].get<speaker::SpeakerId>();
      }

      if (lineRoot.contains("speech_rate")) {
        speechRate = lineRoot["speech_rate"].get<float>();
      }
    }
    catch(const std::exception& e)
    {
      cout << "{\"code\":-2,\"message\":\"input data invalid: " << e.what() << "\"}" << endl;
      continue;
    }

    // Timestamp is used for path to output WAV file
    const auto now = chrono::system_clock::now();
    const auto timestamp =
        chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch())
            .count();

    if (outputType == OUTPUT_DIRECTORY) {
      // Generate path using timestamp
      stringstream outputName;
      outputName << timestamp << ".wav";
      filesystem::path outputPath = runConfig.outputPath.value();
      outputPath.append(outputName.str());
      // Output audio to automatically-named WAV file in a directory
      ofstream audioFile(outputPath.string(), ios::binary);
      speaker::phonemeIdsToWavFile(model, phonemeIds, speechRate, audioFile, result);
      if(!result.success)
      {
        continue;
      }
      cout << "{\"code\":0,\"message\":\"OK\",\"task_id\":"
      << taskId
      << ",\"output_path\":\""
      << outputPath.string()
      << "\"}" << endl;
    } else if (outputType == OUTPUT_RAW) {
      // Raw output to stdout
      mutex mutAudio;
      condition_variable cvAudio;
      bool audioReady = false;
      bool audioFinished = false;
      vector<int16_t> audioBuffer;
      vector<int16_t> sharedAudioBuffer;

      thread rawOutputThread(rawOutputProc, ref(sharedAudioBuffer),
                             ref(mutAudio), ref(cvAudio), ref(audioReady),
                             ref(audioFinished));
      auto audioCallback = [&audioBuffer, &sharedAudioBuffer, &mutAudio,
                            &cvAudio, &audioReady]() {
        // Signal thread that audio is ready
        {
          unique_lock lockAudio(mutAudio);
          copy(audioBuffer.begin(), audioBuffer.end(),
               back_inserter(sharedAudioBuffer));
          audioReady = true;
          cvAudio.notify_one();
        }
      };
      speaker::phonemeIdsToAudio(model, phonemeIds, speechRate, audioBuffer, result,
                         audioCallback);

      if(!result.success)
      {
        continue;
      }

      // Signal thread that there is no more audio
      {
        unique_lock lockAudio(mutAudio);
        audioReady = true;
        audioFinished = true;
        cvAudio.notify_one();
      }

      // Wait for audio output to finish
      spdlog::debug("Waiting for audio to finish playing...");
      rawOutputThread.join();

      cout << "\n{\"code\":0,\"message\":\"OK\"}" << endl;
    }

    spdlog::debug("Real-time factor: {} (infer={} sec, audio={} sec)",
                 result.realTimeFactor, result.inferSeconds,
                 result.audioSeconds);

    // Restore config (--json-input)
    model.config.speakerId = speakerId;

  } // for each line

  return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------

void rawOutputProc(vector<int16_t> &sharedAudioBuffer, mutex &mutAudio,
                   condition_variable &cvAudio, bool &audioReady,
                   bool &audioFinished) {
  vector<int16_t> internalAudioBuffer;
  while (true) {
    {
      unique_lock lockAudio{mutAudio};
      cvAudio.wait(lockAudio, [&audioReady] { return audioReady; });

      if (sharedAudioBuffer.empty() && audioFinished) {
        break;
      }

      copy(sharedAudioBuffer.begin(), sharedAudioBuffer.end(),
           back_inserter(internalAudioBuffer));

      sharedAudioBuffer.clear();

      if (!audioFinished) {
        audioReady = false;
      }
    }

    cout.write((const char *)internalAudioBuffer.data(),
               sizeof(int16_t) * internalAudioBuffer.size());
    cout.flush();
    internalAudioBuffer.clear();
  }

} // rawOutputProc

// ----------------------------------------------------------------------------

void printUsage(char *argv[]) {
  cerr << endl;
  cerr << "usage: " << argv[0] << " [options]" << endl;
  cerr << endl;
  cerr << "options:" << endl;
  cerr << "   -h        --help              show this message and exit" << endl;
  cerr << "   -m  FILE  --model       FILE  path to onnx model file" << endl;
  cerr << "   -c  FILE  --config      FILE  path to model config file "
          "(default: model path + .json)"
       << endl;
  cerr << "   -d  DIR   --output_dir  DIR   path to output directory (default: "
          "cwd)"
       << endl;
  cerr << "   --output_raw                  output raw audio to stdout as it "
          "becomes available"
       << endl;
  cerr << "   -s  NUM   --speaker     NUM   id of speaker (default: 0)" << endl;
  cerr << "   --noise_scale           NUM   generator noise (default: 0.667)"
       << endl;
  cerr << "   --length_scale          NUM   phoneme length (default: 1.0)"
       << endl;
  cerr << "   --noise_w               NUM   phoneme width noise (default: 0.8)"
       << endl;
  cerr << "   --silence_seconds       NUM   seconds of silence after each "
          "sentence (default: 0.2)"
       << endl;
  cerr << "   --debug                       print DEBUG messages to the console"
       << endl;
  cerr << endl;
}

void ensureArg(int argc, char *argv[], int argi) {
  if ((argi + 1) >= argc) {
    printUsage(argv);
    exit(0);
  }
}

// Parse command-line arguments
void parseArgs(int argc, char *argv[], RunConfig &runConfig) {
  optional<filesystem::path> modelConfigPath;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-m" || arg == "--model") {
      ensureArg(argc, argv, i);
      runConfig.modelPath = filesystem::path(argv[++i]);
    } else if (arg == "-c" || arg == "--config") {
      ensureArg(argc, argv, i);
      modelConfigPath = filesystem::path(argv[++i]);
    } else if (arg == "-d" || arg == "--output_dir" || arg == "output-dir") {
      ensureArg(argc, argv, i);
      runConfig.outputType = OUTPUT_DIRECTORY;
      runConfig.outputPath = filesystem::path(argv[++i]);
    } else if (arg == "--output_raw" || arg == "--output-raw") {
      runConfig.outputType = OUTPUT_RAW;
    } else if (arg == "-s" || arg == "--speaker") {
      ensureArg(argc, argv, i);
      runConfig.speakerId = (speaker::SpeakerId)stol(argv[++i]);
    } else if (arg == "--noise_scale" || arg == "--noise-scale") {
      ensureArg(argc, argv, i);
      runConfig.noiseScale = stof(argv[++i]);
    } else if (arg == "--length_scale" || arg == "--length-scale") {
      ensureArg(argc, argv, i);
      runConfig.lengthScale = stof(argv[++i]);
    } else if (arg == "--noise_w" || arg == "--noise-w") {
      ensureArg(argc, argv, i);
      runConfig.noiseW = stof(argv[++i]);
    } else if (arg == "--sentence_silence" || arg == "--sentence-silence") {
      ensureArg(argc, argv, i);
      runConfig.sentenceSilenceSeconds = stof(argv[++i]);
    } else if (arg == "--version") {
      std::cout << speaker::getVersion() << std::endl;
      exit(0);
    } else if (arg == "--debug") {
      // Set DEBUG logging
      spdlog::set_level(spdlog::level::debug);
    } else if (arg == "-h" || arg == "--help") {
      printUsage(argv);
      exit(0);
    }
  }

  // Verify model file exists
  ifstream modelFile(runConfig.modelPath.c_str(), ios::binary);
  if (!modelFile.good()) {
    throw runtime_error("Model file doesn't exist");
  }

  if (!modelConfigPath) {
    auto modelPath = runConfig.modelPath.string();
    runConfig.modelConfigPath =
        filesystem::path(modelPath.replace(modelPath.find(".onnx"), 5, ".json"));
  } else {
    runConfig.modelConfigPath = modelConfigPath.value();
  }

  // Verify model config exists
  ifstream modelConfigFile(runConfig.modelConfigPath.c_str());
  if (!modelConfigFile.good()) {
    throw runtime_error("Model config doesn't exist");
  }
}
