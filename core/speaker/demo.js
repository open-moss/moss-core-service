import { Speaker } from "./index.js";

const speaker = new Speaker({
    modelPath: "../../models/speaker/moss.onnx",  // onnx模型文件
    modelConfigPath: "../../models/speaker/moss.json",  // 模型配置文件
    numThreads: 4,  // 推理线程数
    singleSpeaker: true  // 是否单音色模型
});

await speaker.setVolume(100);  // 设置音量
const result = await speaker.say("让人类永远保持理智，的确是一种奢求", {
    speechRate: 0.8,  // 语音语速
    block: true  // 播放是否阻塞
});
console.log(result);  // 合成结果
