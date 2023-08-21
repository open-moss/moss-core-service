from os import path
from pyaudio import PyAudio, paInt16, paContinue
from loguru import logger

from .build import listener
from lib.listener.utils import load_config

class Listener():

    def __init__(self):
        self.initialized: bool = False
        self.input_accept_callback: function = None
        self.output_callback: function = None

    def initialize(self, config_path):
        logger.info(f"listener version: {listener.get_version()}")
        # 加载配置
        config = load_config(config_path)
        self.config = config
        # 初始化
        listener.init(
            config.sample_rate,
            config.vad_window_frame_size,
            config.vad_threshold,
            config.vad_max_sampling_duration,
            config.chunk_size,
            config.num_threads
        )
        # 加载模型
        self.load_models(config.model_dir_path)
        # 创建音频捕获流
        self.create_capture_stream()
        self.initialized = True
        logger.info(f"listener initialization completed")

    def load_models(self, model_dir_path):
        listener.load_models(
            path.join(path.dirname(__file__), '../../', model_dir_path),
            path.join(path.dirname(__file__), './units')
        )

    def input_accept(self, callback):
        if not self.initialized:
            raise RuntimeError("listener  has not been initialized")
        self.input_accept_callback = callback
    
    def input(self, data):
        if not self.initialized:
            raise RuntimeError("listener  has not been initialized")
        if self.input_accept_callback and not self.input_accept_callback():
            return
        listener.input(data)

    def output(self, callback):
        self.output_callback = callback
        listener.output(lambda result: self.output_callback({
            "start_time": result.start_time,
            "end_time": result.end_time,
            "result": result.result,
            "decode_duration": result.decode_duration,
            "audio_duration": result.audio_duration,
            "rtf": round(result.rtf, 3)
        }))

    def create_capture_stream(self):
        self.capture_target = PyAudio()
        self.capture_stream = self.capture_target.open(
            format=paInt16,
            channels=1,
            rate=self.config.sample_rate,
            input=True,
            input_device_index=(None if not hasattr(self.config, "device") else self.config.device),
            frames_per_buffer=1024,
            stream_callback=self.capture_callback
        )
        self.capture_stream.start_stream()

    def close_capture_stream(self):
        if self.capture_stream.is_stopped():
            return
        self.capture_stream.stop_stream()
        self.capture_stream.close()
        self.capture_stream = None

    def capture_callback(self, input_data, frame_cout, time_info, status):
        self.input(input_data)
        return (input_data, paContinue)
