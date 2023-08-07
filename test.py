import time
from lib.speaker import Speaker

speaker = Speaker()
speaker.initialize(config_path="configs/speaker.yaml")
speaker.say("测试")

time.sleep(10)