import struct
from .build import listener

# print(listener.get_version())

# listener.init(16000, 64, 0.7, 180000, 16, 1)
# listener.load_models("/home/moss/projects/moss-core-service/models/listener", "/home/moss/projects/moss-core-service/lib/listener/units")

# buffer = []
# samples = 0
# index = 0
# with open("/home/moss/projects/moss-core-service/MOSS_020.pcm", "rb") as file:
#     bytes = file.read()

#     for i in range(0, len(bytes), 2):
#         # 从每两个字节中提取一个int16_t的值
#         sample = struct.unpack('<h', bytes[i:i+2])[0]
        
#         # 将int16_t除以32768得到浮点数
#         sample_float = sample / 32768.0
#         buffer.append(sample_float)
#         # if samples >= 512:
#         #     # print("next")
#         #     # print(buffer[index * 64:(index + 1) * 64])
#         #     listener.input(buffer[index * 512:(index + 1) * 512])
#         #     samples = 0
#         #     index = index + 1
#         # samples = samples + 1
# listener.input(buffer)

from os import path
import numpy as np
from pyaudio import PyAudio, paInt16, paContinue
from loguru import logger
from lib.listener.utils import load_config

class Listener():

    def __init__(self):
        self.initialized = False

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

    def load_models(self, model_dir_path):
        listener.load_models(
            path.join(path.dirname(__file__), '../../', model_dir_path),
            path.join(path.dirname(__file__), './units')
        )
    
    def input(self, data):
        if not self.initialized:
            raise RuntimeError("listener  has not been initialized")
        listener.input(data)

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
