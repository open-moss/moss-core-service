from os import path
import time
import requests
import atexit
import numpy as np
from queue import Queue
import threading
from loguru import logger
from pyaudio import PyAudio, paInt16

from .build import speaker
from . import commons
from .utils import load_config
from ..text import text_to_sequence

class Speaker():

    def initialize(self, config_path):
        logger.info(f"speaker version: {speaker.get_version()}")
        # 加载配置
        self.config = load_config(config_path)
        # 非远端合成模式时需加载本地模型
        if self.config.mode != "remote":
            self.load_model(
                self.config.model_path if hasattr(self.config, "model_path") else "model/speaker/moss.onnx",
                self.config.model_config_path if hasattr(self.config, "model_config_path") else "models/speaker/moss.json",
                self.config.num_threads if hasattr(self.config, "num_threads")  else 4
            )
        # 启动回放线程
        self.start_playback_thread()
        logger.info(f"speaker initialization completed")

    def say(self, text, speech_rate = 1.0):
        if len(text) == 0:
            return
        if self.config.mode != "remote":
            buffer, result = self.local_synthesize(text, speech_rate)
        else:
            buffer, result = self.remote_synthesize(text, speech_rate)
        print(len(buffer), result)
        self.playback_queue.put(buffer)
        logger.info(f"speaker say: {text}")

    def abort(self):
        with self.playback_queue.mutex:
            self.playback_queue.queue.clear()
        self.close_playback_stream()

    def load_model(self, model_path, model_config_path, num_threads = 4):
        speaker.load_model(
            path.join(path.dirname(__file__), '../../', model_path),
            path.join(path.dirname(__file__), '../../', model_config_path),
            num_threads
        )

    def text_to_phoneme_ids(self, text, is_symbol = False):
        model_config = self.config.model_config
        text_norm = text_to_sequence(text, model_config.symbols, [] if is_symbol else model_config.data.text_cleaners)
        if model_config.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        return text_norm

    def local_synthesize(self, text, speech_rate = 1.0):
        phoneme_ids = self.text_to_phoneme_ids(text)
        buffer, result = speaker.synthesize(phoneme_ids, speech_rate)
        buffer = np.frombuffer(buffer, dtype=np.int16).tobytes()
        result = {
            "infer_duration": result.infer_duration,
            "audio_duration": result.audio_duration,
            "rtf": result.rtf
        }
        return buffer, result

    def remote_synthesize(self, text, speech_rate = 1.0):
        response = requests.post(self.config.remote_url, json={
            "text": text,
            "speechRate": speech_rate
        })
        if response.status_code != 200:
            raise RuntimeError(f"request remote synthesis failed: {response.content.decode()}")
        if response.headers.get('Content-Type') != 'application/octet-stream':
            raise RuntimeError(f"remote synthesis response content-type invalid: {response.headers.get('Content-Type')}")
        buffer = response.content
        result = {
            "infer_duration": 10,
            "audio_duration": 10,
            "rtf": 0.5
        }
        return buffer, result
    
    def start_playback_thread(self):
        self.playback_target = PyAudio()
        self.playback_queue = Queue(self.config.playback_queue_length if hasattr(self.config, "playback_queue_length") else 100)
        threading.Thread(target=self.playback_thread, daemon=True).start()

    def playback_thread(self):
        self.create_playback_stream()
        while True:
            audio_data = self.playback_queue.get()
            if not self.playback_stream:
                self.create_playback_stream()
            try:
                # if hasattr(self, "say_start_callback"):
                #     self.say_start_callback()
                self.playback_stream.write(audio_data)
                # if hasattr(self, "say_end_callback"):
                #     self.say_end_callback()
            except:
                continue
    
    def create_playback_stream(self):
        self.playback_stream = self.playback_target.open(
            format=paInt16,
            channels=1,
            rate=self.config.sample_rate,
            output=True,
            output_device_index=(None if not hasattr(self.config, "device") else self.config.device)
        )

    def close_playback_stream(self):
        if self.playback_stream.is_stopped():
            return
        self.playback_stream.stop_stream()
        time.sleep(0.1)
        self.playback_stream.close()
        self.playback_stream = None

