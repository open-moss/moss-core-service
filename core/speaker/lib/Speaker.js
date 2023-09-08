import fs from "fs-extra";
import VError from "verror";
import AsyncLock from "async-lock";
import _ from "lodash";
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const speaker = require('../build/Release/speaker');
import textCleaners from "./text_cleaners/index.js";
import ModelConfig from "./ModelConfig.js";

const lock = new AsyncLock();

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
    singleSpeaker;
    audioDeviceName;
    audioMixerName;
    currnetVolume;
    symbolMap = {};
    #initialized = false;

    /**
     * @constructor
     * @param {SpeakerOptions} options - 构造函数
     */
    constructor(options = {}) {
        const { modelPath, modelConfigPath, numThreads, lengthScale, noiseScale, noiseW, singleSpeaker, audioDeviceName, audioMixerName } = options;
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
        this.lengthScale = _.defaultTo(lengthScale, 1.0);
        this.noiseScale = _.defaultTo(noiseScale, 0.667);
        this.noiseW = _.defaultTo(noiseW, 0.8);
        this.singleSpeaker = _.defaultTo(singleSpeaker, false);
        this.audioDeviceName = _.defaultTo(audioDeviceName, "default");
        this.audioMixerName = _.defaultTo(audioMixerName, "PCM");
    }

    /**
     * 合成语音并发声
     * 
     * @param {string} text - 语音文本
     * @param {object} options - 发音选项
     * @param {number} options.speechRate - 语速（0.1-2.0）
     * @param {boolean} options.block - 播放是否阻塞
     * @returns {object} - 合成结果
     */
    async say(text, options = {}) {
        const { speechRate = 1.0, block = false } = options;
        !this.#initialized && await this.#initialize();
        const phonemeIds = this.#textToPhonemeIds(text);
        const {
            inferDuration,  // 推理时长
            audioDuration,  // 音频时长
        } = await speaker.say(phonemeIds, 0, speechRate, block);
        return {
            inferDuration,
            audioDuration,
            realTimeFactor: Math.floor(inferDuration / audioDuration * 1000) / 1000
        }
    }

    /**
     * 设置发声音量
     * 
     * @param {number} volume 音量百分比（0-100）
     */
    async setVolume(volume = 100) {
        !this.#initialized && await this.#initialize();
        await speaker.setVolume(volume);
        this.currnetVolume = volume;
    }

    /**
     * 合成语音
     * 
     * @param {string} text 合成文本
     * @param {object} options - 发音选项
     * @param {number} options.speechRate - 语速（0.1-2.0）
     * @returns {object} - 合成结果
     */
    async synthesize(text, options = {}) {
        const { speechRate = 1.0 } = options;
        !this.#initialized && await this.#initialize();
        const phonemeIds = this.#textToPhonemeIds(text);
        const {
            data,  // 音频数据(Int16Array)
            inferDuration,  // 推理时长
            audioDuration,  // 音频时长
        } = await speaker.synthesize(phonemeIds, 0, speechRate);
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

    /**
     * 初始化
     */
    async #initialize() {
        return lock.acquire("initialize", async () => {
            if(this.#initialized)
                return;
            const { modelPath, modelConfig, numThreads, lengthScale, noiseScale, noiseW, singleSpeaker, audioDeviceName, audioMixerName } = this;
            const { maxWavValue, sampleRate } = modelConfig;
            await speaker.initialize(modelPath, {
                maxWavValue,
                sampleRate,
                lengthScale,
                noiseScale,
                noiseW,
                singleSpeaker
            }, numThreads, audioDeviceName, audioMixerName);
            this.#initialized = true;
        });
    }

}