#include <stdlib.h>
#include <variant>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <node_api.h>
#include "speaker.hpp"

#define ASSERT(expr) \
  { \
    if ((expr) == 0) { \
        std::string errorMsg = "Failed assertion: " #expr " at "; \
        errorMsg += __FILE__; \
        errorMsg += ":"; \
        errorMsg += std::to_string(__LINE__); \
        napi_throw_error(env, "100", errorMsg.c_str()); \
    } \
  }

typedef struct {
    napi_async_work work;
    napi_deferred deferred;
    void* args;
} PromiseData;

typedef struct {
    std::string modelPath;
    std::string modelConfigPath;
    int32_t numThreads;
} LoadModelArguments;

typedef struct {
    std::vector<int64_t> *phonemeIds;
    double speechRate;
    std::vector<int16_t> audioBuffer;
    speaker::SynthesisResult result;
} SynthesizeArguments;

static napi_value getVersionWrapper(napi_env env, napi_callback_info info)
{
    napi_value result;
    std::string version = speaker::getVersion();
    ASSERT(napi_create_string_utf8(env, version.c_str(), NAPI_AUTO_LENGTH, &result) == napi_ok)
    return result;
}

static void parseToString(napi_env env, napi_value value, std::string *result)
{
    size_t size{};
    ASSERT(napi_get_value_string_utf8(env, value, nullptr, 0, &size) == napi_ok)
    result->assign(size, ' ');
    ASSERT(napi_get_value_string_utf8(env, value, &(*result)[0], size + 1, nullptr) == napi_ok)
}

static napi_value loadModelWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    try
    {
        size_t argc = 3;
        napi_value argv[3];
        ASSERT(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok)
        if (argc < 3)
        {
            napi_throw_error(env, "101", "Invalid arguments");
        }

        LoadModelArguments* args = new LoadModelArguments();
        PromiseData* promiseData =  new PromiseData();
        promiseData->args = args;

        std::string modelPath;
        std::string modelConfigPath;
        int32_t numThreads;
        parseToString(env, argv[0], &args->modelPath);
        parseToString(env, argv[1], &args->modelConfigPath);
        ASSERT(napi_get_value_int32(env, argv[2], &args->numThreads) == napi_ok)
        

        ASSERT(napi_create_promise(env, &(promiseData->deferred), &promise) == napi_ok)

        napi_value workName;
        ASSERT(napi_create_string_utf8(env, "loadModel", NAPI_AUTO_LENGTH, &workName) == napi_ok)
        ASSERT(napi_create_async_work(env, nullptr, workName, 
            [](napi_env env, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                LoadModelArguments* args = (LoadModelArguments*)promiseData->args;
                speaker::loadModel(
                    args->modelPath,
                    args->modelConfigPath,
                    static_cast<int16_t>(args->numThreads)
                );
                napi_resolve_deferred(env, static_cast<napi_deferred>(promiseData->deferred), nullptr);
            },
            [](napi_env env, napi_status status, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                napi_delete_async_work(env, static_cast<napi_async_work>(promiseData->work));
                delete (LoadModelArguments*)promiseData->args;
                delete promiseData;
            },
            promiseData, &(promiseData->work)
        ) == napi_ok)
        
        ASSERT(napi_queue_async_work(env, promiseData->work) == napi_ok)
        return promise;
    }
    catch (const std::exception& e)
    {
        napi_value errorMsg;
        ASSERT(napi_create_string_utf8(env, e.what(), NAPI_AUTO_LENGTH, &errorMsg) == napi_ok)
        napi_deferred deferred;
        ASSERT(napi_create_promise(env, &deferred, &promise) == napi_ok)
        ASSERT(napi_reject_deferred(env, deferred, errorMsg) == napi_ok)
        return promise;
    }
}

static napi_value synthesizeWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    try
    {
        size_t argc = 4;
        napi_value argv[4];
        ASSERT(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok)
        if (argc < 4)
        {
            napi_throw_error(env, "101", "Invalid arguments");
        }
        
        SynthesizeArguments* args = new SynthesizeArguments();
        PromiseData* promiseData =  new PromiseData();
        promiseData->args = args;

        size_t bufferLength;
        void* buffer;
        napi_get_buffer_info(env, argv[0], &buffer, &bufferLength);
        const int64_t* int64Data = static_cast<const int64_t*>(buffer);
        size_t numElements = bufferLength / sizeof(int64_t);
        std::vector<int64_t> phonemeIds(int64Data, int64Data + numElements);
        args->phonemeIds = &phonemeIds;
        ASSERT(napi_get_value_double(env, argv[1], &args->speechRate) == napi_ok)

        ASSERT(napi_create_promise(env, &(promiseData->deferred), &promise) == napi_ok)

        napi_value workName;
        ASSERT(napi_create_string_utf8(env, "synthesize", NAPI_AUTO_LENGTH, &workName) == napi_ok)
        ASSERT(napi_create_async_work(env, nullptr, workName, 
            [](napi_env env, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                SynthesizeArguments* args = (SynthesizeArguments*)promiseData->args;
                speaker::synthesize(
                    *args->phonemeIds,
                    static_cast<float>(args->speechRate),
                    args->audioBuffer,
                    args->result
                );
                napi_resolve_deferred(env, static_cast<napi_deferred>(promiseData->deferred), nullptr);
            },
            [](napi_env env, napi_status status, void* data) {
                PromiseData* promiseData = (PromiseData*)data;
                napi_delete_async_work(env, static_cast<napi_async_work>(promiseData->work));
                delete (SynthesizeArguments*)promiseData->args;
                delete promiseData;
            },
            promiseData, &(promiseData->work)
        ) == napi_ok)
        
        ASSERT(napi_queue_async_work(env, promiseData->work) == napi_ok)
        return promise;
    }
    catch (const std::exception& e)
    {
        napi_value errorMsg;
        ASSERT(napi_create_string_utf8(env, e.what(), NAPI_AUTO_LENGTH, &errorMsg) == napi_ok)
        napi_deferred deferred;
        ASSERT(napi_create_promise(env, &deferred, &promise) == napi_ok)
        ASSERT(napi_reject_deferred(env, deferred, errorMsg) == napi_ok)
        return promise;
    }
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value getVersionFn, loadModelFn, synthesizeFn;
    ASSERT(napi_create_function(env, "getVersion", NAPI_AUTO_LENGTH, getVersionWrapper, nullptr, &getVersionFn) == napi_ok)
    ASSERT(napi_set_named_property(env, exports, "getVersion", getVersionFn) == napi_ok)
    ASSERT(napi_create_function(env, "loadModel", NAPI_AUTO_LENGTH, loadModelWrapper, nullptr, &loadModelFn) == napi_ok)
    ASSERT(napi_set_named_property(env, exports, "loadModel", loadModelFn) == napi_ok)
    ASSERT(napi_create_function(env, "synthesize", NAPI_AUTO_LENGTH, synthesizeWrapper, nullptr, &synthesizeFn) == napi_ok)
    ASSERT(napi_set_named_property(env, exports, "synthesize", synthesizeFn) == napi_ok)
    return exports;
}

NAPI_MODULE("speaker", Init)
