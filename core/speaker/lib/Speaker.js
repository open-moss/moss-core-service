import fs from "fs-extra";
import VError from "verror";
import _ from "lodash";
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const speaker = require('../build/Release/speaker');
import textCleaners from "./text_cleaners/index.js";
import ModelConfig from "./ModelConfig.js";

/**
 * @typedef {Object} SpeakerOptions
 * @property {string} modelPath - 模型路径
 * @property {string} modelConfigPath - 模型配置路径
 * @property {number} numThreads - 推理线程数
 * @property {number} lengthScale - 时长缩放
 * @property {number} noiseScale - 
 * @property {number} noiseW - 
 */

export default class Speaker {

    modelPath;
    modelConfigPath;
    modelConfig;
    numThreads;
    lengthScale;
    noiseScale;
    noiseW;
    symbolMap = {};
    #modelLoaded = false;

    /**
     * @constructor
     * @param {SpeakerOptions} options - 构造函数
     */
    constructor(options = {}) {
        const { modelPath, modelConfigPath, numThreads, lengthScale = 1.0, noiseScale = 0.667, noiseW = 0.8 } = options;
        if(!_.isString(modelPath) || !fs.pathExistsSync(modelPath))
            throw new VError("model file not found: %s", modelPath || "");
        if(!_.isString(modelConfigPath) || !fs.pathExistsSync(modelConfigPath))
            throw new VError("model config file not found: %s", modelConfigPath || "");
        if(!_.isFinite(numThreads))
            throw new VError("inference num threads invalid");
        this.modelPath = modelPath;
        this.modelConfigPath = modelConfigPath;
        this.modelConfig = new ModelConfig(fs.readJSONSync(modelConfigPath));
        this.modelConfig.symbols.forEach((symbol, index) => this.symbolMap[symbol] = index);
        this.numThreads = numThreads;
        this.lengthScale = lengthScale;
        this.noiseScale = noiseScale;
        this.noiseW = noiseW;
    }

    async say(text, speechRate = 1.0) {
        !this.#modelLoaded && await this.#loadModel();
        const phonemeIds = this.#textToPhonemeIds(text);
        console.log(phonemeIds);
        await speaker.say(phonemeIds, 0, speechRate);
    }

    async synthesize(text, speechRate = 1.0) {
        !this.#modelLoaded && await this.#loadModel();
        const phonemeIds = this.#textToPhonemeIds(text);
        console.log(phonemeIds);
        const {
            data,  // 音频数据(Int16Array)
            inferDuration,  // 推理时长
            audioDuration,  // 音频时长
        } = await speaker.synthesize(phonemeIds, 0, speechRate);
        console.log(data, inferDuration, audioDuration, Math.floor(inferDuration / audioDuration * 1000) / 1000);
        fs.writeFileSync("a.pcm", Buffer.from(data.buffer));
        return {
            data,
            inferDuration,
            audioDuration,
            realTimeFactor: Math.floor(inferDuration / audioDuration * 1000) / 1000
        }
    }

    #textToPhonemeIds(text) {
        const { textCleanerNames } = this.modelConfig;
        const phonemeIds = [];
        text = textCleanerNames.reduce((str, cleanersName) => textCleaners[cleanersName] ? textCleaners[cleanersName](str) : str, text);
        for(let char of text) {
            if(_.isUndefined(this.symbolMap[char]))
                continue;
            phonemeIds.push(this.symbolMap[char]);
        }
        const phonemeIdsPadding = [];
        for(let i = 0;i < phonemeIds.length * 2;i += 2) {
            phonemeIdsPadding[i] = 0;
            phonemeIdsPadding[i + 1] = phonemeIds[Math.ceil(i / 2)];
        }
        return new Int16Array(phonemeIdsPadding);
    }

    async #loadModel() {
        const { modelPath, modelConfig, numThreads, lengthScale, noiseScale, noiseW } = this;
        const { maxWavValue, sampleRate } = modelConfig;
        await speaker.loadModel(modelPath, {
            maxWavValue,
            sampleRate,
            lengthScale,
            noiseScale,
            noiseW
        }, numThreads);
        this.#modelLoaded = true;
    }

}