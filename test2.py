import time
from lib.listener import Listener
from lib.speaker import Speaker


speaker = Speaker()
speaker.initialize(config_path="configs/speaker.yaml")

listener = Listener()
listener.initialize(config_path="configs/listener.yaml")
listener.input_accept(lambda: not speaker.speaking)

# speaker.say("今天天气不错哈哈哈哈哈哈")
# time.sleep(1)
# # speaker.abort()
# speaker.say("重新开始说话")

# with open("MOSS_020.pcm", "rb") as file:
#     buffer = file.read()
    
# # def outputCallback(data):
#     # speaker.say(data.result)


# speaker.sayStart(lambda: listener.pause())
# speaker.sayEnd(lambda: listener.resume())
listener.output(lambda result: print(result))
# time.sleep(5)
# listener.input(buffer)
speaker.say("你行不行啊")

time.sleep(3600 * 8)