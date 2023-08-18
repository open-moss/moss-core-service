#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "speaker.hpp"

namespace py = pybind11;

std::tuple<py::array_t<int16_t>, speaker::SynthesisResult> synthesize(std::vector<int64_t> &phonemeIds, float speechRate)
{
    std::vector<int16_t> audioBuffer;
    speaker::SynthesisResult result;
    speaker::synthesize(phonemeIds, speechRate, audioBuffer, result);
    return std::make_tuple(py::array_t<int16_t>(audioBuffer.size(), audioBuffer.data()), result);
}

PYBIND11_MODULE(speaker, m)
{
    py::class_<speaker::SynthesisResult>(m, "SynthesisResult")
        .def(py::init<>())
        .def_readwrite("infer_duration", &speaker::SynthesisResult::inferDuration)
        .def_readwrite("audio_duration", &speaker::SynthesisResult::audioDuration)
        .def_readwrite("rtf", &speaker::SynthesisResult::realTimeFactor);

    m.def("get_version", &speaker::getVersion, "get speaker version");
    m.def("load_model", &speaker::loadModel, "load vits onnx model");
    m.def("synthesize", &synthesize, "synthesize audio");
}
