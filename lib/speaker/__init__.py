from os import path
from subprocess import PIPE, Popen, check_output
from queue import Queue
import numpy as np
import json, time
import threading
import pyaudio
from loguru import logger
import lib.speaker.commons as commons
from lib.speaker.utils import load_config, SpeakerConfig, SpeakerMessage
from lib.speaker.text import text_to_sequence

class Speaker():

    def initialize(self, config_path):
        self.config = load_config(config_path)
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
        thread = threading.Thread(target=self.speaker_thread, daemon=True)
        thread.start()
        self.speaker_ready_event.wait()

        self.speaker_lock = threading.Lock()

        self.playback_queue = Queue(100)
        threading.Thread(target=self.playback_thread, daemon=True).start()

        audio = pyaudio.PyAudio()
        self.stream = audio.open(format=pyaudio.paInt16,
                    channels=1,
                    rate=16000,
                    output=True,
                    output_device_index=2)
        self.stream.start_stream()
        
        with open("./prompt.txt", 'rb') as f:
            line = f.readline()
            while line:
                text = line.decode().strip('\n')
                if text != '':
                    print(text)
                    self.text_to_speech(text)
                line = f.readline()

        time.sleep(120)
        logger.info(f"speaker executable process is running")

    def text_to_speech(self, text, speech_rate = 1.0):
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
        finally:
            self.speaker_lock.release()

    def text_to_phoneme_ids(self, text, is_symbol = False):
        model_config = self.config.model_config
        text_norm = text_to_sequence(text, model_config.symbols, [] if is_symbol else model_config.data.text_cleaners)
        if model_config.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        return text_norm

    def playback_thread(self):
        while True:
            audio_data = self.playback_queue.get()
            self.stream.write(audio_data)
            
        # stream.write(audio_data.tobytes())
        # stream.stop_stream()
        # stream.close()
        # audio.terminate()
    
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
            "0" if not config.speaker_id else f"{config.speaker_id}"
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

