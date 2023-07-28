from os import path
from subprocess import PIPE, Popen, check_output
from torch import LongTensor
import scipy.io.wavfile as wavf
import numpy as np
import json, time
import threading
from loguru import logger 
import lib.speaker.commons as commons
import lib.speaker.utils as utils
from lib.speaker.text import text_to_sequence

class Speaker():

    def initialize(self, config_path):
        self.config = utils.load_config(config_path)
        self.speaker_ready = False
        self.speaker_path = path.join(path.dirname(__file__), "build/speaker")
        if not path.exists(self.speaker_path):
            raise FileExistsError(f"speaker executable is not found: {self.speaker_path}\n# please rebuild lib/speaker:\n$ cd lib/speaker\n$ make")
        try:
            self.speaker_version = check_output(f"{self.speaker_path} --version", shell=True).decode().replace("\n", "")
        except Exception as e:
            raise ChildProcessError(f"speaker executable test error: {e}")
        logger.info(f"speaker executable version: {self.speaker_version}")

        thread = threading.Thread(
            target=self.speaker_handler,
            daemon=True
        )
        thread.start()
        # thread.join()
        time.sleep(3)
        self.text_to_speech()
        time.sleep(10)
        logger.info(f"speaker executable process is running")

    def text_to_speech(self):
        if not self.speaker_ready:
            raise RuntimeError("speaker process is not running")
        self.speaker_process.stdin.write('{"phoneme_ids":[0,47,0,11,0,28,0,47,0,49,0,21,0,38,0,47,0,32,0,47,0,1,0,49,0,26,0,42,0,39,0,45,0,49,0,7,0,42,0,45,0,23,0,48,0,49,0,24,0,42,0,29,0,47,0,20,0,41,0,34,0,46,0,1,0,49,0,40,0,45,0,23,0,47,0,49,0,34,0,47,0,9,0,36,0,47,0,2,0]}\n'.encode())
        self.speaker_process.stdin.flush()

    def get_text(self, text, hps, is_symbol):
        text_norm = text_to_sequence(text, hps.symbols, [] if is_symbol else hps.data.text_cleaners)
        if hps.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        text_norm = LongTensor(text_norm)
        return text_norm
    
    def speaker_handler(self):
        (model_path, model_config_path, speaker_id) = self.config.values()
        self.speaker_ready = False
        self.speaker_process = Popen([
            self.speaker_path,
            "-m",
            path.join(path.dirname(__file__), '../../', model_path if model_path else "models/speaker/moss.onnx"),
            "-c",
            path.join(path.dirname(__file__), '../../', model_config_path if model_config_path else "models/speaker/moss.json"),
            "-s",
            "0" if not speaker_id else f"{speaker_id}"
        ], stdin=PIPE, stdout=PIPE, stderr=PIPE)
        buffer = b""
        while(True):
            raw = self.speaker_process.stdout.readline()
            if raw[0] == ord("{"):
                try:
                    jsonData = json.loads(raw.decode())
                    data = SpeakerData(**jsonData)
                except:
                    continue
                logger.debug(f"speaker: {jsonData}")
                if data.code == 0 and not self.speaker_ready:
                    self.speaker_ready = True
                    continue
                if data.code == 0:
                    print(np.frombuffer(buffer, dtype=np.int16))
                    wavf.write("test.wav", 16000, np.frombuffer(buffer, dtype=np.int16))
            else:
                buffer += raw[:-1]

class SpeakerData():
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            if type(v) == dict:
                v = SpeakerData(**v)
            self[k] = v
        if(type(self.code) != int):
            raise ValueError("speaker reply code invalid")
        if(self.message is None):
            raise ValueError("speaker reply message invalid")

    def keys(self):
        return self.__dict__.keys()

    def items(self):
        return self.__dict__.items()

    def values(self):
        return self.__dict__.values()

    def __len__(self):
        return len(self.__dict__)

    def __getitem__(self, key):
        return getattr(self, key)

    def __setitem__(self, key, value):
        return setattr(self, key, value)

    def __contains__(self, key):
        return key in self.__dict__

    def __repr__(self):
        return self.__dict__.__repr__()

