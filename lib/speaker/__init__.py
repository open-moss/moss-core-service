from os import path
from subprocess import PIPE, Popen, check_output
from queue import Queue
import numpy as np
import json, time
import threading
from pyaudio import PyAudio, paInt16
from loguru import logger
import lib.speaker.commons as commons
from lib.speaker.utils import load_config, SpeakerMessage
from lib.speaker.text import text_to_sequence

class Speaker():

    def initialize(self, config_path):
        # 加载配置
        self.config = load_config(config_path)
        # 启动合成线程
        self.start_speaker_thread()
        # 启动回放线程
        self.start_playback_thread()
        
        logger.info(f"speaker executable process is running")

    def say(self, text, speech_rate = 1.0):
        if not self.speaker_ready:
            raise RuntimeError("speaker process is not running")
        self.speaker_lock.acquire(blocking=True)
        try:
            self.speaker_finished_event = threading.Event()
            phoneme_ids = self.text_to_phoneme_ids(text)
            message = json.dumps({
                "phoneme_ids": phoneme_ids,
                "speech_rate": speech_rate
            })
            message += "\n"
            self.speaker_process.stdin.write(message.encode())
            self.speaker_process.stdin.flush()
            self.speaker_finished_event.wait()
            logger.info(f"speaker say: {text}")
        finally:
            self.speaker_lock.release()

    def abort(self):
        with self.playback_queue.mutex:
            self.playback_queue.queue.clear()
        self.close_playback_stream()

    def text_to_phoneme_ids(self, text, is_symbol = False):
        model_config = self.config.model_config
        text_norm = text_to_sequence(text, model_config.symbols, [] if is_symbol else model_config.data.text_cleaners)
        if model_config.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        return text_norm

    def create_playback_stream(self):
        self.playback_stream = self.playback_target.open(
            format=paInt16,
            channels=1,
            rate=16000,
            output=True
        )
        self.playback_stream.start_stream()

    def close_playback_stream(self):
        if self.playback_stream.is_stopped():
            return
        self.playback_stream.stop_stream()
        time.sleep(0.1)
        self.playback_stream.close()
        self.playback_stream = None

    def start_speaker_thread(self):
        self.speaker_path = path.join(path.dirname(__file__), "build/speaker")
        if not path.exists(self.speaker_path):
            raise FileExistsError(f"speaker executable is not found: {self.speaker_path}\n# please rebuild lib/speaker:\n$ cd lib/speaker\n$ make")
        try:
            self.speaker_version = check_output(f"{self.speaker_path} --version", shell=True).decode().replace("\n", "")
        except Exception as e:
            raise ChildProcessError(f"speaker executable test error: {e}")
        logger.info(f"speaker executable version: {self.speaker_version}")
        self.speaker_ready = False
        self.speaker_ready_event = threading.Event()
        self.speaker_lock = threading.Lock()
        thread = threading.Thread(target=self.speaker_thread, daemon=True)
        thread.start()
        self.speaker_ready_event.wait()

    def start_playback_thread(self):
        self.playback_target = PyAudio()
        self.playback_queue = Queue(self.config.playback_queue_length if self.config.playback_queue_length else 100)
        threading.Thread(target=self.playback_thread, daemon=True).start()

    def playback_thread(self):
        self.create_playback_stream()
        while True:
            audio_data = self.playback_queue.get()
            if not self.playback_stream:
                self.create_playback_stream()
            try:
                self.playback_stream.write(audio_data)
            except:
                continue
    
    def speaker_thread(self):
        config = self.config
        self.speaker_ready = False
        self.speaker_process = Popen([
            self.speaker_path,
            "-m",
            path.join(path.dirname(__file__), '../../', config.model_path if config.model_path else "models/speaker/moss.onnx"),
            "-c",
            path.join(path.dirname(__file__), '../../', config.model_config_path if config.model_config_path else "models/speaker/moss.json"),
            "-s",
            "0" if not config.speaker_id else f"{config.speaker_id}",
            "--num_threads",
            "4"
        ], stdin=PIPE, stdout=PIPE, stderr=PIPE)
        buffer = bytearray()
        while True:
            raw = self.speaker_process.stdout.readline()
            if raw[0] == ord("{") and raw[-2] == ord("}"):
                try:
                    jsonData = json.loads(raw.decode())
                    message = SpeakerMessage(**jsonData)
                except:
                    continue
                logger.debug(f"speaker: {jsonData}")
                if message.code == 0:
                    if not self.speaker_ready:
                        self.speaker_ready = True
                        self.speaker_ready_event.set()
                        continue
                    if len(buffer) % 2 != 0:
                        buffer.extend(bytearray(b"\n"))
                    audio_data = np.frombuffer(buffer, dtype=np.int16)
                    buffer = bytearray()
                    self.playback_queue.put(audio_data.tobytes())
                    self.speaker_finished_event.set()
            else:
                buffer.extend(bytearray(raw))

