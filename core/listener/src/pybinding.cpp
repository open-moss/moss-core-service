#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "listener.hpp"

namespace py = pybind11;

PYBIND11_MODULE(listener, m)
{
    m.doc() = "listener python bindings";

    py::class_<listener::DecodeResult>(m, "DecodeResult")
        .def(py::init<>())
        .def_readwrite("start_time", &listener::DecodeResult::startTime)
        .def_readwrite("end_time", &listener::DecodeResult::endTime)
        .def_readwrite("result", &listener::DecodeResult::result)
        .def_readwrite("decode_duration", &listener::DecodeResult::decodeDuration)
        .def_readwrite("audio_duration", &listener::DecodeResult::audioDuration)
        .def_readwrite("rtf", &listener::DecodeResult::realTimeFactor);

    m.def("get_version", &listener::getVersion, "get listener version");
    m.def("init", &listener::init, "init listener");
    m.def("load_models", &listener::loadModels, "load onnx models");
    m.def("input", &listener::input, "input pcm data");
    m.def("output", &listener::output, "output decode result");

}
