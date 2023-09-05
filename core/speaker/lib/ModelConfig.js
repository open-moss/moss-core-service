export default class ModelConfig {

    #data = {};

    constructor(data = {}) {
        this.#data = data;
    }

    get train() {
        return this.#data["train"] || {};
    }

    get data() {
        return this.#data["data"] || {};
    }

    get model() {
        return this.#data["model"] || {};
    }

    get textCleanerNames() {
        return this.data["text_cleaners"] || [];
    }

    get maxWavValue() {
        return this.data["max_wav_value"] || 32768;
    }

    get sampleRate() {
        return this.data["sampling_rate"] || 16000;
    }

    get addBlank() {
        return this.data["add_blank"] || true;
    }

    get nSpeakers() {
        return this.data["n_speakers"] || 1;
    }

    get speakers() {
        let speakers = this.#data["speakers"] || {}
        if(Array.isArray(speakees)) {
            speakers = {};
            this.#data["speakers"].forEach((value, index) => speakers[value] = index);
        }
        return speakers;
    }

    get symbols() {
        return this.#data["symbols"] || [];
    }

}