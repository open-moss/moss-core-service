import time
from lib.speaker import Speaker

speaker = Speaker()
speaker.initialize(config_path="configs/speaker.yaml")
with open("./prompt.txt", 'rb') as f:
    line = f.readline()
    while line:
        text = line.decode().strip('\n')
        if text != '':
            speaker.say(text)
        line = f.readline()

time.sleep(120)