#include <iostream>
#include <vector>
#include <functional>
#include <sstream>
#include <cstring>
#include <chrono>
#include <cmath>

#include "vad.h"

VadIterator::VadIterator(int sampleRate, int frameSize, float _threshold, int minSilenceDurationMS, int speechPadMS)
{
    sample_rate = sampleRate;
    sr_per_ms = sample_rate / 1000;
    threshold = _threshold;
    min_silence_samples = sr_per_ms * minSilenceDurationMS;
    speech_pad_samples = sr_per_ms * speechPadMS;
    window_size_samples = frameSize * sr_per_ms;
    
    input.resize(window_size_samples);
    input_node_dims[0] = 1;
    input_node_dims[1] = window_size_samples;
    // std::cout << "== Input size" << input.size() << std::endl;
    _h.resize(size_hc);
    _c.resize(size_hc);
    sr.resize(1);
    sr[0] = sample_rate;
}

void VadIterator::loadModel(const std::string &model_path, int inter_threads, int intra_threads)
{   
    session_options.SetIntraOpNumThreads(intra_threads);
    session_options.SetInterOpNumThreads(inter_threads);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options.DisableCpuMemArena();
    session_options.DisableMemPattern();
    session_options.DisableProfiling();
    session_options.SetLogSeverityLevel(3);
    // Load model
    session = std::make_shared<Ort::Session>(env, model_path.c_str(), session_options);
}

int VadIterator::getCurrentTime()
{
    return static_cast<int>(round((1.0 * (current_sample + window_size_samples + speech_pad_samples) / sample_rate) * 1000));
}

void VadIterator::resetStates()
{
    // Call reset before each audio start
    std::memset(_h.data(), 0.0f, _h.size() * sizeof(float));
    std::memset(_c.data(), 0.0f, _c.size() * sizeof(float));
    triggerd = false;
    temp_end = 0;
    current_sample = 0;
}

void VadIterator::predict(const std::vector<float> &data, const std::function<void(int)>& startCallback, const std::function<void(int)>& endCallback)
{
   
    // Infer
    // Create ort tensors
    input.assign(data.begin(), data.end());
    Ort::Value input_ort = Ort::Value::CreateTensor<float>(
        memory_info, input.data(), input.size(), input_node_dims, 2);
    Ort::Value sr_ort = Ort::Value::CreateTensor<int64_t>(
        memory_info, sr.data(), sr.size(), sr_node_dims, 1);
    Ort::Value h_ort = Ort::Value::CreateTensor<float>(
        memory_info, _h.data(), _h.size(), hc_node_dims, 3);
    Ort::Value c_ort = Ort::Value::CreateTensor<float>(
        memory_info, _c.data(), _c.size(), hc_node_dims, 3);

    // Clear and add inputs
    ort_inputs.clear();
    ort_inputs.emplace_back(std::move(input_ort));
    ort_inputs.emplace_back(std::move(sr_ort));
    ort_inputs.emplace_back(std::move(h_ort));
    ort_inputs.emplace_back(std::move(c_ort));

    // Infer
    ort_outputs = session->Run(
        Ort::RunOptions{nullptr},
        input_node_names.data(), ort_inputs.data(), ort_inputs.size(),
        output_node_names.data(), output_node_names.size());

    // Output probability & update h,c recursively
    float output = ort_outputs[0].GetTensorMutableData<float>()[0];
    float *hn = ort_outputs[1].GetTensorMutableData<float>();
    std::memcpy(_h.data(), hn, size_hc * sizeof(float));
    float *cn = ort_outputs[2].GetTensorMutableData<float>();
    std::memcpy(_c.data(), cn, size_hc * sizeof(float));

    // Push forward sample index
    current_sample += window_size_samples;
    
    // Reset temp_end when > threshold 
    if ((output >= threshold) && (temp_end != 0))
    {
        temp_end = 0;
    }
    // 1) Silence
    if ((output < threshold) && (triggerd == false))
    {
        // printf("{ silence: %.3f s }\n", 1.0 * current_sample / sample_rate);
    }
    // 2) Speaking 
    if ((output >= (threshold - 0.15)) && (triggerd == true))
    {
        // printf("{ speaking_2: %.3f s }\n", 1.0 * current_sample / sample_rate);
    }

    // 3) Start
    if ((output >= threshold) && (triggerd == false))
    {
        triggerd = true;
        speech_start = current_sample - window_size_samples - speech_pad_samples; // minus window_size_samples to get precise start time point.
        // printf("{ start: %.3f s }\n", 1.0 * speech_start / sample_rate);
        startCallback(static_cast<int>(round((1.0 * speech_start / sample_rate) * 1000)));
    }

    // 4) End 
    if ((output < (threshold - 0.15)) && (triggerd == true))
    {

        if (temp_end == 0)
        {
            temp_end = current_sample;
        }
        // a. silence < min_slience_samples, continue speaking 
        if ((current_sample - temp_end) < min_silence_samples)
        {
            // printf("{ speaking_4: %.3f s }\n", 1.0 * current_sample / sample_rate);
            // printf("");
        }
        // b. silence >= min_slience_samples, end speaking
        else
        {
            speech_end = temp_end ? temp_end + speech_pad_samples : current_sample + speech_pad_samples;
            temp_end = 0;
            triggerd = false;
            // printf("{ end: %.3f s }\n", 1.0 * speech_end / sample_rate);
            endCallback(static_cast<int>(round((1.0 * speech_end / sample_rate) * 1000)));
        }
    }

}
