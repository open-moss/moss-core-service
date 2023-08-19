from os import path
import time
import requests
import numpy as np
from queue import Queue
import threading
from loguru import logger
from pyaudio import PyAudio, paInt16

from .build import speaker
from . import commons
from .utils import load_config, SpeakerConfig
from ..text import text_to_sequence

class Speaker():

    def __init__(self):
        self.initialized = False
        self.synthesizer_ready = False
        self.speaking: bool = False
        self.config: SpeakerConfig = None
        self.playback_target: PyAudio = None
        self.playback_queue: Queue = None
        self.playback_stream = None

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
        else:
            self.synthesizer_ready = True
        # 初始化PyAudio
        self.playback_target = PyAudio()
        # 初始化回放队列
        self.playback_queue = Queue(self.config.playback_queue_length if hasattr(self.config, "playback_queue_length") else 100)
        # 启动回放线程
        threading.Thread(target=self._playback_thread, daemon=True).start()
        self.initialized = True
        logger.info(f"speaker initialization completed")

    def say(self, text, speech_rate = 1.0):
        if not self.initialized:
            raise RuntimeError("speaker has not been initialized")
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
        if not self.initialized:
            raise RuntimeError("speaker has not been initialized")
        with self.playback_queue.mutex:
            self.playback_queue.queue.clear()
        self._close_playback_stream()

    def load_model(self, model_path, model_config_path, num_threads = 4):
        speaker.load_model(
            path.join(path.dirname(__file__), '../../', model_path),
            path.join(path.dirname(__file__), '../../', model_config_path),
            num_threads
        )
        self.synthesizer_ready = True

    def text_to_phoneme_ids(self, text, is_symbol = False):
        model_config = self.config.model_config
        text_norm = text_to_sequence(text, model_config.symbols, [] if is_symbol else model_config.data.text_cleaners)
        if model_config.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        return text_norm

    def local_synthesize(self, text, speech_rate = 1.0):
        if not self.synthesizer_ready:
            raise RuntimeError("speaker synthesizer has been not ready")
        phoneme_ids = self.text_to_phoneme_ids(text)
        buffer, result = speaker.synthesize(phoneme_ids, speech_rate)
        buffer = np.frombuffer(buffer, dtype=np.int16).tobytes()
        result = {
            "infer_duration": round(result.infer_duration, 2),
            "audio_duration": round(result.audio_duration, 2),
            "rtf": round(result.rtf, 3)
        }
        return buffer, result

    def remote_synthesize(self, text, speech_rate = 1.0):
        if not self.synthesizer_ready:
            raise RuntimeError("speaker synthesizer has been not ready")
        start_time = time.time()
        response = requests.post(self.config.remote_url, json={
            "text": text,
            "speechRate": speech_rate
        })
        infer_duration = time.time() - start_time
        if response.status_code != 200:
            raise RuntimeError(f"request remote synthesis failed: {response.content.decode()}")
        if response.headers.get('Content-Type') != 'application/octet-stream':
            raise RuntimeError(f"remote synthesis response content-type invalid: {response.headers.get('Content-Type')}")
        buffer = response.content
        audio_duration = len(buffer) / 2 / self.config.sample_rate
        result = {
            "infer_duration": round(infer_duration, 2),
            "audio_duration": round(audio_duration, 2),
            "rtf": round(infer_duration / audio_duration, 3)
        }
        return buffer, result

    def _playback_thread(self):
        self._create_playback_stream()
        while True:
            buffer = self.playback_queue.get()
            if not self.playback_stream:
                self._create_playback_stream()
            try:
                self.speaking = True
                self.playback_stream.write(buffer)
                self.speaking = False
            except:
                continue
    
    def _create_playback_stream(self):
        self.playback_stream = self.playback_target.open(
            format=paInt16,
            channels=1,
            rate=self.config.sample_rate,
            output=True,
            output_device_index=(None if not hasattr(self.config, "device") else self.config.device)
        )

    def _close_playback_stream(self):
        if self.playback_stream.is_stopped():
            return
        self.playback_stream.stop_stream()
        self.playback_stream.close()
        self.playback_stream = None
