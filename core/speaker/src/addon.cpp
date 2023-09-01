#include <stdlib.h>
#include <node_api.h>
#include "speaker.hpp"

#define CHECK(expr) \
  { \
    if ((expr) == 0) { \
    } \
  }

typedef struct {
    napi_async_work work;
    napi_deferred deferred;

} AddonData;

napi_status parseToString(napi_env env, napi_value value, std::string *result)
{
    size_t size{};
    napi_get_value_string_utf8(env, value, nullptr, 0, &size);
    result->assign(size, ' ');
    return napi_get_value_string_utf8(env, value, &(*result)[0], size + 1, nullptr);
}

napi_value getVersionWrapper(napi_env env, napi_callback_info info)
{
    napi_value result;
    std::string version = speaker::getVersion();
    napi_create_string_utf8(env, version.c_str(), NAPI_AUTO_LENGTH, &result);
    return result;
}

napi_value loadModelWrapper(napi_env env, napi_callback_info info)
{
    napi_value promise;
    size_t argc = 3;
    napi_value argv[3];
    CHECK(napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok);
    if (argc < 3)
    {
        napi_value errorMsg;
        napi_create_string_utf8(env, "Invalid arguments", NAPI_AUTO_LENGTH, &errorMsg);
        napi_deferred deferred;
        napi_create_promise(env, &deferred, &promise);
        napi_reject_deferred(env, deferred, errorMsg);
        return promise;
    }

    std::string modelPath;
    std::string modelConfigPath;
    int32_t numThreads;
    parseToString(env, argv[0], &modelPath);
    parseToString(env, argv[1], &modelConfigPath);
    napi_get_value_int32(env, argv[2], &numThreads);

    AddonData* addonData = (AddonData*)malloc(sizeof(AddonData));
    
    CHECK(napi_create_promise(env, &(addonData->deferred), &promise) == napi_ok);

    napi_async_work work;
    napi_value workName;
    napi_create_string_utf8(env, "loadModel", NAPI_AUTO_LENGTH, &workName);
    CHECK(napi_create_async_work(env, nullptr, workName, 
        [](napi_env env, void* data) {
            AddonData* addonData = (AddonData*)data;
            // speaker::loadModel(modelPath, modelConfigPath, numThreads);
            napi_resolve_deferred(env, static_cast<napi_deferred>(addonData->deferred), nullptr);
        },
        [](napi_env env, napi_status status, void* data) {
            AddonData* addonData = (AddonData*)data;
            napi_delete_async_work(env, static_cast<napi_async_work>(addonData->work));
            free(addonData);
        },
        addonData, &(addonData->work)) == napi_ok);
    
    napi_queue_async_work(env, work);
    return promise;
}

napi_value Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value getVersionFn, loadModelFn;
    CHECK(napi_create_function(env, "getVersion", NAPI_AUTO_LENGTH, getVersionWrapper, nullptr, &getVersionFn) == napi_ok);
    CHECK(napi_set_named_property(env, exports, "getVersion", getVersionFn) == napi_ok);
    CHECK(napi_create_function(env, "loadModel", NAPI_AUTO_LENGTH, loadModelWrapper, nullptr, &loadModelFn) == napi_ok);
    CHECK(napi_set_named_property(env, exports, "loadModel", loadModelFn) == napi_ok);
    return exports;
}

NAPI_MODULE("speaker", Init)