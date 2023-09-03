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
        const phonemeIds = await this.textToPhonemeIds(text);
        console.log(phonemeIds);
        const {
            data,  // 音频数据(Int16Array)
            inferDuration,  // 推理时长
            audioDuration,  // 音频时长
        } = await speaker.synthesize(phonemeIds, speechRate);
        fs.writeFileSync("a.pcm", Buffer.from(data.buffer));
        console.log(data, inferDuration, audioDuration, inferDuration / audioDuration);
    }

    async textToPhonemeIds(text) {
        const phonemeIds = [];
        for(let char of textReplacement(text)) {
            if(!this.symbolMap.has(char))
                continue;
            phonemeIds.push(this.symbolMap.get(char));
        }
        const phonemeIdsPadding = [];
        for(let i = 0;i < phonemeIds.length * 2;i += 2) {
            phonemeIdsPadding[i] = 0;
            phonemeIdsPadding[i + 1] = phonemeIds[Math.ceil(i / 2)];
        }
        return new Int16Array(phonemeIdsPadding);
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
