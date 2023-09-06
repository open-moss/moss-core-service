find_path(ONNXRUNTIME_INCLUDE_DIR
  NAMES onnxruntime_cxx_api.h
  PATHS
    /usr/include
    /usr/local/include
)

find_library(ONNXRUNTIME_LIBRARY
  NAMES onnxruntime
  PATHS
    /usr/lib
    /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime DEFAULT_MSG
  ONNXRUNTIME_INCLUDE_DIR
  ONNXRUNTIME_LIBRARY
)

if(ONNXRuntime_FOUND)
  set(ONNXRuntime_FOUND TRUE)
endif()

mark_as_advanced(ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY)
