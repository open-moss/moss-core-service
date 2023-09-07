#include <stdlib.h>
#include <variant>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <node_api.h>
#include "speaker.hpp"

#define ASSERT(expr) \
  { \
    if ((expr)) { \
        std::string errorMsg = "Failed assertion: " #expr " at "; \
        errorMsg += __FILE__; \
        errorMsg += ":"; \
        errorMsg += std::to_string(__LINE__); \
        napi_throw_error(env, "100", errorMsg.c_str()); \
    } \
  }

/**
 * Promise数据
 */
struct PromiseData {
    napi_async_work work;  // NAPI异步任务
    napi_deferred deferred;  // NAPI Promise状态
    void* args;  // 参数指针
};

/**
 * loadModel参数
 */
struct LoadModelArguments {
    std::string modelPath;  // 模型路径
    speaker::ModelConfig modelConfig;  // 模型配置路径
    int32_t numThreads;  // 推理线程数
};

/**
 * synthesize参数
 */
struct SynthesizeArguments {
    std::vector<int64_t> *phonemeIds;  // 音素数组
    int speakerId;  // 音色ID
    double speechRate;  // 语速
    std::vector<int16_t> audioBuffer;  // 音频数据
    speaker::SynthesisResult result;  // 合成结果
    ~SynthesizeArguments() {
        if (phonemeIds != nullptr) {
            delete phonemeIds;
        }
    }  // 析构
};

/**
 * say参数
 */
struct SayArguments {
    std::vector<int64_t> *phonemeIds;  // 音素数组
    int speakerId;  // 音色ID
    double speechRate;  // 语速
    ~SayArguments() {
        if (phonemeIds != nullptr) {
            delete phonemeIds;
        }
    }  // 析构
};

/**
 * 转换js值为string
 */
static void parseToString(napi_env env, napi_value value, std::string *result)
{
    size_t size{};
    ASSERT(napi_get_value_string_utf8(env, value, nullptr, 0, &size))
    result->assign(size, ' ');
    ASSERT(napi_get_value_string_utf8(env, value, &(*result)[0], size + 1, nullptr))
}

/**
 * 解析模型配置
 */
static void parseToModelConfig(napi_env env, napi_value value, speaker::ModelConfig &modelConfig)
{
    bool isObject;
    napi_valuetype valueType;
    napi_typeof(env, value, &valueType);
    if(valueType != napi_object)
    {
        napi_throw_type_error(env, "102", "Invalid model config");
    }
    napi_value maxWavValueName, sampleRateName, lengthScaleName, noiseScaleName, noiseWName;
    ASSERT(napi_create_string_utf8(env, "maxWavValue", NAPI_AUTO_LENGTH, &maxWavValueName))
    ASSERT(napi_create_string_utf8(env, "sampleRate", NAPI_AUTO_LENGTH, &sampleRateName))
    ASSERT(napi_create_string_utf8(env, "lengthScale", NAPI_AUTO_LENGTH, &lengthScaleName))
    ASSERT(napi_create_string_utf8(env, "noiseScale", NAPI_AUTO_LENGTH, &noiseScaleName))
    ASSERT(napi_create_string_utf8(env, "noiseW", NAPI_AUTO_LENGTH, &noiseWName))
    napi_value _maxWavValue, _sampleRate, _lengthScale, _noiseScale, _noiseW;
    napi_get_property(env, value, maxWavValueName, &_maxWavValue);
    napi_get_property(env, value, sampleRateName, &_sampleRate);
    napi_get_property(env, value, lengthScaleName, &_lengthScale);
    napi_get_property(env, value, noiseScaleName, &_noiseScale);
    napi_get_property(env, value, noiseWName, &_noiseW);
    double maxWavValue, lengthScale, noiseScale, noiseW;
    napi_get_value_double(env, _maxWavValue, &maxWavValue);
    modelConfig.maxWavValue = static_cast<float>(maxWavValue);
    napi_get_value_int32(env, _sampleRate, &(modelConfig.sampleRate));
    napi_get_value_double(env, _lengthScale, &lengthScale);
    modelConfig.lengthScale = static_cast<float>(lengthScale);
    napi_get_value_double(env, _noiseScale, &noiseScale);
    modelConfig.noiseScale = static_cast<float>(noiseScale);
    napi_get_value_double(env, _noiseW, &noiseW);
    modelConfig.noiseW = static_cast<float>(noiseW);
}

/**
 * loadModel函数包装
 */
static napi_value loadModelWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    try
    {
        size_t argc = 3;
        napi_value argv[3];
        ASSERT(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr))
        if (argc < 3)
        {
            ASSERT(napi_throw_error(env, "101", "Invalid arguments"));
        }

        LoadModelArguments* args = new LoadModelArguments();
        PromiseData* promiseData =  new PromiseData();
        promiseData->args = args;

        std::string modelPath;
        int32_t numThreads;
        parseToString(env, argv[0], &args->modelPath);
        parseToModelConfig(env, argv[1], args->modelConfig);
        ASSERT(napi_get_value_int32(env, argv[2], &args->numThreads))

        ASSERT(napi_create_promise(env, &(promiseData->deferred), &promise))

        napi_value workName;
        ASSERT(napi_create_string_utf8(env, "loadModel", NAPI_AUTO_LENGTH, &workName))
        ASSERT(napi_create_async_work(env, nullptr, workName, 
            [](napi_env env, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                LoadModelArguments* args = (LoadModelArguments*)promiseData->args;
                speaker::loadModel(
                    args->modelPath,
                    args->modelConfig,
                    static_cast<int16_t>(args->numThreads)
                );
            },
            [](napi_env env, napi_status status, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                napi_value result;
                ASSERT(napi_get_undefined(env, &result))
                ASSERT(napi_resolve_deferred(env, static_cast<napi_deferred>(promiseData->deferred), result));
                ASSERT(napi_delete_async_work(env, static_cast<napi_async_work>(promiseData->work)));
                delete (LoadModelArguments*)promiseData->args;
                delete promiseData;
            },
            promiseData, &(promiseData->work)
        ))
        
        ASSERT(napi_queue_async_work(env, promiseData->work))
        return promise;
    }
    catch (const std::exception& e)
    {
        napi_value errorMsg;
        ASSERT(napi_create_string_utf8(env, e.what(), NAPI_AUTO_LENGTH, &errorMsg))
        napi_deferred deferred;
        ASSERT(napi_create_promise(env, &deferred, &promise))
        ASSERT(napi_reject_deferred(env, deferred, errorMsg))
        return promise;
    }
}

/**
 * synthesize函数包装
 */
static napi_value synthesizeWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    try
    {
        size_t argc = 3;
        napi_value argv[3];
        ASSERT(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr))
        bool isTypedArray;
        napi_is_typedarray(env, argv[0], &isTypedArray);
        if (argc < 3 || !isTypedArray)
        {
            ASSERT(napi_throw_error(env, "101", "Invalid arguments"));
        }
        
        SynthesizeArguments* args = new SynthesizeArguments();
        PromiseData* promiseData = new PromiseData();
        promiseData->args = args;

        napi_typedarray_type type;
        size_t arrayLength;
        int16_t* array;
        ASSERT(napi_get_typedarray_info(env, argv[0], nullptr, &arrayLength, (void**)(&array), nullptr, nullptr));
        std::vector<int64_t>* phonemeIds = new std::vector<int64_t>(arrayLength);
        for (uint32_t i = 0; i < arrayLength; ++i) {
            (*phonemeIds)[i] = static_cast<int64_t>(array[i]);
        }
        args->phonemeIds = phonemeIds;
        ASSERT(napi_get_value_int32(env, argv[1], &args->speakerId))
        ASSERT(napi_get_value_double(env, argv[2], &args->speechRate))

        ASSERT(napi_create_promise(env, &(promiseData->deferred), &promise))

        napi_value workName;
        ASSERT(napi_create_string_utf8(env, "synthesize", NAPI_AUTO_LENGTH, &workName))
        ASSERT(napi_create_async_work(env, nullptr, workName, 
            [](napi_env env, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                SynthesizeArguments* args = (SynthesizeArguments*)promiseData->args;
                speaker::synthesize(
                    *(args->phonemeIds),
                    static_cast<int16_t>(args->speakerId),
                    static_cast<float>(args->speechRate),
                    args->audioBuffer,
                    args->result
                );
            },
            [](napi_env env, napi_status status, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                SynthesizeArguments* args = (SynthesizeArguments*)promiseData->args;
                int16_t* buffer = args->audioBuffer.data();
                int bufferLength = args->audioBuffer.size();
                void* tempData;
                napi_value arrayBuffer;
                ASSERT(napi_create_arraybuffer(env, bufferLength * sizeof(int16_t), &tempData, &arrayBuffer));
                memcpy(tempData, buffer, bufferLength * sizeof(int16_t));
                napi_value array;
                ASSERT(napi_create_typedarray(env, napi_int16_array, bufferLength, arrayBuffer, 0, &array));
                napi_value result;
                ASSERT(napi_create_object(env, &result));
                ASSERT(napi_set_named_property(env, result, "data", array));
                napi_value inferDuration, audioDuration;
                ASSERT(napi_create_int32(env, args->result.inferDuration, &inferDuration));
                ASSERT(napi_create_int32(env, args->result.audioDuration, &audioDuration));
                ASSERT(napi_set_named_property(env, result, "inferDuration", inferDuration));
                ASSERT(napi_set_named_property(env, result, "audioDuration", audioDuration));
                ASSERT(napi_resolve_deferred(env, static_cast<napi_deferred>(promiseData->deferred), result));
                ASSERT(napi_delete_async_work(env, static_cast<napi_async_work>(promiseData->work)));
                delete args;
                delete promiseData;
            },
            promiseData, &(promiseData->work)
        ))
        
        ASSERT(napi_queue_async_work(env, promiseData->work))
        return promise;
    }
    catch (const std::exception& e)
    {
        napi_value errorMsg;
        ASSERT(napi_create_string_utf8(env, e.what(), NAPI_AUTO_LENGTH, &errorMsg))
        napi_deferred deferred;
        ASSERT(napi_create_promise(env, &deferred, &promise))
        ASSERT(napi_reject_deferred(env, deferred, errorMsg))
        return promise;
    }
}

/**
 * say函数包装
 */
static napi_value sayWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    try
    {
        size_t argc = 3;
        napi_value argv[3];
        ASSERT(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr))
        bool isTypedArray;
        napi_is_typedarray(env, argv[0], &isTypedArray);
        if (argc < 3 || !isTypedArray)
        {
            ASSERT(napi_throw_error(env, "101", "Invalid arguments"));
        }
        
        SayArguments* args = new SayArguments();
        PromiseData* promiseData = new PromiseData();
        promiseData->args = args;

        napi_typedarray_type type;
        size_t arrayLength;
        int16_t* array;
        ASSERT(napi_get_typedarray_info(env, argv[0], nullptr, &arrayLength, (void**)(&array), nullptr, nullptr));
        std::vector<int64_t>* phonemeIds = new std::vector<int64_t>(arrayLength);
        for (uint32_t i = 0; i < arrayLength; ++i) {
            (*phonemeIds)[i] = static_cast<int64_t>(array[i]);
        }
        args->phonemeIds = phonemeIds;
        ASSERT(napi_get_value_int32(env, argv[1], &args->speakerId))
        ASSERT(napi_get_value_double(env, argv[2], &args->speechRate))

        ASSERT(napi_create_promise(env, &(promiseData->deferred), &promise))

        napi_value workName;
        ASSERT(napi_create_string_utf8(env, "say", NAPI_AUTO_LENGTH, &workName))
        ASSERT(napi_create_async_work(env, nullptr, workName, 
            [](napi_env env, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                SayArguments* args = (SayArguments*)promiseData->args;
                speaker::say(
                    *(args->phonemeIds),
                    static_cast<int16_t>(args->speakerId),
                    static_cast<float>(args->speechRate)
                );
            },
            [](napi_env env, napi_status status, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                napi_value result;
                ASSERT(napi_get_undefined(env, &result))
                ASSERT(napi_resolve_deferred(env, static_cast<napi_deferred>(promiseData->deferred), result));
                ASSERT(napi_delete_async_work(env, static_cast<napi_async_work>(promiseData->work)));
                delete (SayArguments*)promiseData->args;
                delete promiseData;
            },
            promiseData, &(promiseData->work)
        ))
        
        ASSERT(napi_queue_async_work(env, promiseData->work))
        return promise;
    }
    catch (const std::exception& e)
    {
        napi_value errorMsg;
        ASSERT(napi_create_string_utf8(env, e.what(), NAPI_AUTO_LENGTH, &errorMsg))
        napi_deferred deferred;
        ASSERT(napi_create_promise(env, &deferred, &promise))
        ASSERT(napi_reject_deferred(env, deferred, errorMsg))
        return promise;
    }
}

/**
 * NAPI模块初始化
 */
NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
    napi_value loadModelFn, synthesizeFn, sayFn;

    // loadModel函数包装暴露
    ASSERT(napi_create_function(env, "loadModel", NAPI_AUTO_LENGTH, loadModelWrapper, nullptr, &loadModelFn))
    ASSERT(napi_set_named_property(env, exports, "loadModel", loadModelFn))

    // synthesize函数包装暴露
    ASSERT(napi_create_function(env, "synthesize", NAPI_AUTO_LENGTH, synthesizeWrapper, nullptr, &synthesizeFn))
    ASSERT(napi_set_named_property(env, exports, "synthesize", synthesizeFn))

    // say函数包装暴露
    ASSERT(napi_create_function(env, "say", NAPI_AUTO_LENGTH, sayWrapper, nullptr, &sayFn))
    ASSERT(napi_set_named_property(env, exports, "say", sayFn))

    return exports;
}
