import fs from "fs";
import path from "path";
import { createRequire } from 'module';
import textReplacement from "./text/index.js";
const require = createRequire(import.meta.url);
const speaker = require('./build/Release/speaker');

const PACK_INFO = JSON.parse(fs.readFileSync(path.join(path.resolve(), "package.json")).toString());

/**
 * @typedef {Object} SpeakerOptions
 * @property {string} modelPath - 模型路径
 * @property {string} modelConfigPath - 模型配置路径
 * @property {number} numThreads - 推理线程数
 * @property {string[]} symbols - 符号数组
 */

class Speaker {

    modelPath;
    modelConfigPath;
    numThreads;
    symbolMap = new Map();
    #modelLoaded = false;

    /**
     * @constructor
     * @param {SpeakerOptions} options - 构造函数
     */
    constructor(options = {}) {
        if(Speaker.instance)
            return Speaker.instance;
        Speaker.instance = this;
        const { modelPath, modelConfigPath, numThreads, symbols = [] } = options;
        this.modelPath = modelPath;
        this.modelConfigPath = modelConfigPath;
        this.numThreads = numThreads;
        symbols.forEach((symbol, index) => this.symbolMap.set(symbol, index));
    }

    async say(text, speechRate = 1.0) {
        !this.#modelLoaded && await this.#loadModel();
        const phonemeIds = this.textToPhonemeIds(text);
        console.log(phonemeIds);
        // const {
        //     data,  // 音频数据(Int16Array)
        //     inferDuration,  // 推理时长
        //     audioDuration,  // 音频时长
        //     realTimeFactor  // 推理实时率
        // } = await speaker.synthesize(phonemeIds, speechRate);
        // console.log(data, inferDuration, audioDuration, realTimeFactor);
    }

    textToPhonemeIds(text) {
        const phonemeIds = [];
        for(let char of textReplacement(text)) {
            if(!this.symbolMap.has(char))
                continue;
            phonemeIds.push(this.symbolMap.get(char));
        }
        return new Int16Array(phonemeIds);
        // return new Int16Array([
        //     0, 47, 0, 11, 0, 28, 0, 47, 0, 49, 0, 21, 0, 38, 0, 47, 0, 32, 0, 47, 0, 1, 0, 49, 0, 26, 0, 42, 0, 39, 0, 45, 0, 49, 0, 7, 0, 42, 0, 45, 0, 23, 0, 48, 0, 49, 0, 24, 0, 42, 0, 29, 0, 47, 0, 20, 0, 41, 0, 34, 0, 46, 0, 1, 0, 49, 0, 40, 0, 45, 0, 23, 0, 47, 0, 49, 0, 34, 0, 47, 0, 9, 0, 36, 0, 47, 0, 2, 0
        // ]);
    }

    async #loadModel() {
        const { modelPath, modelConfigPath, numThreads } = this;
        await speaker.loadModel(modelPath, modelConfigPath, numThreads);
        this.#modelLoaded = true;
    }

    get version() {
        return PACK_INFO["version"] || "unknown";
    }

}

export default Speaker;

// (async () => {
//     await speaker.loadModel("/home/moss/projects/moss-core-service/models/speaker/moss.onnx", "/home/moss/projects/moss-core-service/models/speaker/moss.json", 4);
//     console.log(await speaker.synthesize(new Int16Array([
//         0, 47, 0, 11, 0, 28, 0, 47, 0, 49, 0, 21, 0, 38, 0, 47, 0, 32, 0, 47, 0, 1, 0, 49, 0, 26, 0, 42, 0, 39, 0, 45, 0, 49, 0, 7, 0, 42, 0, 45, 0, 23, 0, 48, 0, 49, 0, 24, 0, 42, 0, 29, 0, 47, 0, 20, 0, 41, 0, 34, 0, 46, 0, 1, 0, 49, 0, 40, 0, 45, 0, 23, 0, 47, 0, 49, 0, 34, 0, 47, 0, 9, 0, 36, 0, 47, 0, 2, 0
//     ]), 1.0));
//     console.log("AAA");
// })()
// .catch(err => {
//     console.error(err);
// });
