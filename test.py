import time
from lib.speaker import Speaker

speaker = Speaker()
speaker.initialize(config_path="configs/speaker.yaml")
speaker.say("地球已成功脱离木星，moss此次使命已完成，期待您的下一次唤醒。")

time.sleep(120)