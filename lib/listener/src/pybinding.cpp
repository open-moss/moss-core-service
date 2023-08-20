#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "listener.hpp"

namespace py = pybind11;

PYBIND11_MODULE(listener, m)
{

    m.def("get_version", &listener::getVersion, "get listener version");
    m.def("init", &listener::init, "init listener");
    m.def("load_models", &listener::loadModels, "load onnx models");
    m.def("input", &listener::input, "input pcm data");

}
