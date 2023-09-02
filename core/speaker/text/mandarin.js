import { pinyin, PINYIN_STYLE } from "@napi-rs/pinyin";

const LATIN_TO_BOPOMOFO_PAIRS = new Map([
    ["a", "ㄟˉ"],
    ["b", "ㄅㄧˋ"],
    ["c", "ㄙㄧˉ"],
    ["d", "ㄉㄧˋ"],
    ["e", "ㄧˋ"],
    ["f", "ㄝˊㄈㄨˋ"],
    ["g", "ㄐㄧˋ"],
    ["h", "ㄝˇㄑㄩˋ"],
    ["i", "ㄞˋ"],
    ["j", "ㄐㄟˋ"],
    ["k", "ㄎㄟˋ"],
    ["l", "ㄝˊㄛˋ"],
    ["m", "ㄝˊㄇㄨˋ"],
    ["n", "ㄣˉ"],
    ["o", "ㄡˉ"],
    ["p", "ㄆㄧˉ"],
    ["q", "ㄎㄧㄡˉ"],
    ["r", "ㄚˋ"],
    ["s", "ㄝˊㄙˋ"],
    ["t", "ㄊㄧˋ"],
    ["u", "ㄧㄡˉ"],
    ["v", "ㄨㄧˉ"],
    ["w", "ㄉㄚˋㄅㄨˋㄌㄧㄡˋ"],
    ["x", "ㄝˉㄎㄨˋㄙˋ"],
    ["y", "ㄨㄞˋ"],
    ["z", "ㄗㄟˋ"]
]);

export function numberToChinese(text) {

}

export function chineseToBopomofo(text) {
    text = text.replace(/、|；|：/g, "，");
    const words = pinyin(text, {
        style: PINYIN_STYLE.WithTone,
        // heteronym: true,
        segment: true
    });
    text = '';
    console.log(words);
    // for(let word of words) {
        
    // }
    return text;
}

export function latinToBopmofo(text) {
    let _text = "";
    for(let char of text) {
        if(char >= 'A' && char <= 'Z')
            char = char.toLowerCase();
        if(!LATIN_TO_BOPOMOFO_PAIRS.has(char)) {
            _text += char;
            continue;
        }
        _text += LATIN_TO_BOPOMOFO_PAIRS.get(char);
    }
    return _text;
}

