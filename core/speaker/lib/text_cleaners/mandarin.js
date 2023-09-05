import jieba from "@node-rs/jieba";
import { pinyin } from "pinyin-pro";
import n2words from "n2words";
import pinyinToBopomofo from "./pinyin-to-bopomofo.js";

const FIND_NUMBER_REGEXP = /\d+(?:\.?\d+)?[年]?/g;

export function numberToChinese(text) {
    text = text.replace(FIND_NUMBER_REGEXP, match => {
        if(match.at(-1) == "年")
            match = match.slice(0, -1);
        return n2words(match, { lang: "zh" });
    });
    return text;
}

export function chineseToBopomofo(text) {
    text = text.replace(/、|；|：/g, "，");
    const words = jieba.cut(text);
    text = "";
    for(let word of words) {
        if(!/[\u4e00-\u9fff]/.test(word)) {
            text += word;
            continue;
        }
        const tones = pinyin(word, { toneType: "num", type: "array" });
        text += tones.reduce((str, tone) => {
            const bopomofo = pinyinToBopomofo(tone);
            return str + bopomofo.replace(/([\u3105-\u3129])$/, "$1ˉ");
        }, "");
    }
    return text;
}
