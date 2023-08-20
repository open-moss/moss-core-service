import struct
from .build import listener

print(listener.get_version())

listener.init(16000, 32, 0.6, 180000, 16, 1)
listener.load_models("/home/moss/projects/moss-core-service/models/listener", "/home/moss/projects/moss-core-service/lib/listener_binding/units")

buffer = []
samples = 0
index = 0
with open("/home/moss/projects/moss-core-service/MOSS_020.pcm", "rb") as file:
    bytes = file.read()

    for i in range(0, len(bytes), 2):
        # 从每两个字节中提取一个int16_t的值
        sample = struct.unpack('<h', bytes[i:i+2])[0]
        
        # 将int16_t除以32768得到浮点数
        sample_float = sample / 32768.0
        buffer.append(sample_float)
        if samples >= 512:
            # print("next")
            # print(buffer[index * 64:(index + 1) * 64])
            listener.input(buffer[index * 512:(index + 1) * 512])
            samples = 0
            index = index + 1
        samples = samples + 1

# from os import path
# from subprocess import PIPE, Popen, check_output
# import atexit
# import json, time
# import threading
# from pyaudio import PyAudio, paInt16, paContinue
# from loguru import logger
# from lib.listener.utils import load_config, ListenerMessage

# class Listener():

#     def initialize(self, config_path):
#         self._pause = False
#         # 加载配置
#         self.config = load_config(config_path)
#         # 启动语音识别线程
#         self.start_listener_thread()
#         # 创建音频捕获流
#         self.create_capture_stream()

#         logger.info(f"listener executable process is running")

#     def pause(self):
#         self._pause = True

#     def resume(self):
#         self._pause = False

#     def input(self, data):
#         if not self.listener_ready:
#             raise RuntimeError("listener process is not running")
#         self.listener_process.stdin.write(data)
#         self.listener_process.stdin.flush()
    
#     def output(self, callback):
#         self.output_callback = callback

#     def create_capture_stream(self):
#         self.capture_target = PyAudio()
#         self.capture_stream = self.capture_target.open(
#             format=paInt16,
#             channels=1,
#             rate=self.config.sample_rate,
#             input=True,
#             input_device_index=(None if not hasattr(self.config, "device") else self.config.device),
#             frames_per_buffer=1024,
#             stream_callback=self.capture_callback
#         )
#         self.capture_stream.start_stream()

#     def close_capture_stream(self):
#         if self.capture_stream.is_stopped():
#             return
#         self.capture_stream.stop_stream()
#         time.sleep(0.1)
#         self.capture_stream.close()
#         self.capture_stream = None

#     def capture_callback(self, input_data, frame_cout, time_info, status):
#         if self._pause:
#             return (input_data, paContinue)
#         self.input(input_data)
#         return (input_data, paContinue)

#     def start_listener_thread(self):
#         self.listener_path = path.join(path.dirname(__file__), "build/listener")
#         if not path.exists(self.listener_path):
#             raise FileExistsError(f"listener executable is not found: {self.listener_path}\n# please rebuild lib/listener:\n$ cd lib/listener\n$ make -j")
#         try:
#             self.listener_version = check_output(f"{self.listener_path} --version", shell=True).decode().replace("\n", "")
#         except Exception as e:
#             raise ChildProcessError(f"listener executable test error: {e}")
#         self.listener_unit_path = path.join(path.dirname(__file__), "units")
#         if not path.exists(self.listener_unit_path):
#             raise FileExistsError(f"listener units is not found: {self.listener_unit_path}")
#         logger.info(f"listener executable version: {self.listener_version}")
#         self.listener_ready = False
#         self.listener_ready_event = threading.Event()
#         self.listener_lock = threading.Lock()
#         thread = threading.Thread(target=self.listener_thread, daemon=True)
#         thread.start()
#         self.listener_ready_event.wait()

#     def listener_thread(self):
#         config = self.config
#         self.listener_ready = False
#         self.listener_process = Popen([
#             self.listener_path,
#             "--onnx_dir",
#             path.join(path.dirname(__file__), '../../', config.model_dir_path if config.model_dir_path else "models/listener"),
#             "--unit_path",
#             self.listener_unit_path,
#             "--sample_rate",
#             "16000" if not hasattr(config, "sample_rate") else f"{config.sample_rate}",
#             "--num_threads",
#             "1" if not hasattr(config, "num_threads") else f"{config.num_threads}",
#             "--chunk_size",
#             "16" if not hasattr(config, "chunk_size") else f"{config.chunk_size}",
#             "--vad_threshold",
#             "0.6" if not hasattr(config, "vad_threshold") else f"{config.vad_threshold}",
#             "--vad_window_frame_size",
#             "64" if not hasattr(config, "vad_window_frame_size") else f"{config.vad_window_frame_size}",
#             "--vad_max_sampling_duration",
#             "180000" if not hasattr(config, "vad_max_sampling_duration") else f"{config.vad_max_sampling_duration}"
#         ], stdin=PIPE, stdout=PIPE, stderr=PIPE)
#         atexit.register(lambda: self.listener_process.terminate() if self.listener_process.poll() is None else None)
#         while True:
#             raw = self.listener_process.stdout.readline()
#             if len(raw) == 0 or raw[0] != ord("{") or raw[-2] != ord("}"):
#                 logger.debug(f"listener[invalid]: {raw}")
#                 continue
#             try:
#                 jsonData = json.loads(raw.decode())
#                 logger.debug(f"listener: {jsonData}")
#                 message = ListenerMessage(**jsonData)
#             except:
#                 logger.debug(f"listener[invalid]: {raw}")
#                 continue
#             if message.code != 0:
#                 continue
#             if message.data.event == "wait_input":
#                 self.listener_ready = True
#                 self.listener_ready_event.set()
#                 continue
#             if message.data.event == "decode_end":
#                 if message.data.result == "":
#                     continue
#                 if hasattr(self, "output_callback"):
#                     self.output_callback(message.data)
#             else:
#                 continue
