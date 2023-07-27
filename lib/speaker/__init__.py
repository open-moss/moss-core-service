from os import path, system
from subprocess import PIPE, Popen, check_output
from torch import LongTensor
from loguru import logger 
import lib.speaker.commons as commons
import lib.speaker.utils as utils
from lib.speaker.text import text_to_sequence

class Speaker():

    def initialize(self, config_path):
        self.config = utils.load_config(config_path)
        self.speaker_path = path.join(path.dirname(__file__), "build/speaker")
        if not path.exists(self.speaker_path):
            raise FileExistsError(f"speaker executable is not found: {self.speaker_path}\n# please rebuild lib/speaker:\n$ cd lib/speaker\n$ make")
        try:
            self.speaker_version = check_output(f"{self.speaker_path} --version", shell=True).decode().replace("\n", "")
        except Exception as e:
            raise ChildProcessError(f"speaker executable test error: {e}")
        logger.info(f"speaker executable version: {self.speaker_version}")
        (model_path, model_config_path, speaker_id) = self.config.values()

        self.speaker_process = Popen([
            self.speaker_path,
            "-m",
            path.join(path.dirname(__file__), '../../', model_path if model_path else "models/speaker/moss.onnx"),
            "-c",
            path.join(path.dirname(__file__), '../../', model_config_path if model_config_path else "models/speaker/moss.json"),
            "-s",
            "0" if not speaker_id else f"{speaker_id}"
        ])

    def text_to_speech():
        pass

    def get_text(text, hps, is_symbol):
        text_norm = text_to_sequence(text, hps.symbols, [] if is_symbol else hps.data.text_cleaners)
        if hps.data.add_blank:
            text_norm = commons.intersperse(text_norm, 0)
        text_norm = LongTensor(text_norm)
        return text_norm