from os import path
from subprocess import PIPE, Popen, check_output
from queue import Queue
import json
import threading
from loguru import logger
from lib.listener.utils import load_config, ListenerMessage

class Listener():

    def initialize(self, config_path):
        # 加载配置
        self.config = load_config(config_path)
        self.start_listener_thread()
        logger.info(f"listener executable process is running")

    def start_listener_thread(self):
        self.listener_path = path.join(path.dirname(__file__), "build/listener")
        if not path.exists(self.listener_path):
            raise FileExistsError(f"listener executable is not found: {self.listener_path}\n# please rebuild lib/listener:\n$ cd lib/listener\n$ make -j")
        try:
            self.listener_version = check_output(f"{self.listener_path} --version", shell=True).decode().replace("\n", "")
        except Exception as e:
            raise ChildProcessError(f"listener executable test error: {e}")
        logger.info(f"listener executable version: {self.listener_version}")
        self.listener_ready = False
        self.listener_ready_event = threading.Event()
        self.listener_lock = threading.Lock()
        thread = threading.Thread(target=self.listener_thread, daemon=True)
        thread.start()
        self.listener_ready_event.wait()

    def listener_thread(self):
        config = self.config
        self.listener_ready = False
        self.listener_process = Popen([
            self.listener_path,
            "--onnx_dir",
            path.join(path.dirname(__file__), '../../', config.model_dir_path if config.model_dir_path else "models/listener"),
            "--unit_path",
            path.join(path.dirname(__file__), '../../', config.model_dir_path if config.model_dir_path else "models/listener/units.txt"),
            "--chunk_size",
            "16" if not config.chunk_size else f"{config.chunk_size}"
        ], stdin=PIPE, stdout=PIPE, stderr=PIPE)
        while True:
            raw = self.listener_process.stdout.readline()
            if raw[0] != ord("{") or raw[-2] != ord("}"):
                continue
            try:
                jsonData = json.loads(raw.decode())
                message = ListenerMessage(**jsonData)
            except:
                continue
            logger.debug(f"listener: {jsonData}")
            if message.code != 0:
                continue
            if message.data.event == "wait_input":
                self.listener_ready = True
                self.listener_ready_event.set()
                continue
            if message.data.event != "decode_end":
                continue
            

            

