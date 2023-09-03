const LATIN_TO_BOPOMOFO_PAIRS = {
    "a": "ㄟˉ",
    "b": "ㄅㄧˋ",
    "c": "ㄙㄧˉ",
    "d": "ㄉㄧˋ",
    "e": "ㄧˋ",
    "f": "ㄝˊㄈㄨˋ",
    "g": "ㄐㄧˋ",
    "h": "ㄝˇㄑㄩˋ",
    "i": "ㄞˋ",
    "j": "ㄐㄟˋ",
    "k": "ㄎㄟˋ",
    "l": "ㄝˊㄛˋ",
    "m": "ㄝˊㄇㄨˋ",
    "n": "ㄣˉ",
    "o": "ㄡˉ",
    "p": "ㄆㄧˉ",
    "q": "ㄎㄧㄡˉ",
    "r": "ㄚˋ",
    "s": "ㄝˊㄙˋ",
    "t": "ㄊㄧˋ",
    "u": "ㄧㄡˉ",
    "v": "ㄨㄧˉ",
    "w": "ㄉㄚˋㄅㄨˋㄌㄧㄡˋ",
    "x": "ㄝˉㄎㄨˋㄙˋ",
    "y": "ㄨㄞˋ",
    "z": "ㄗㄟˋ"
};

export default function(text) {
    let _text = "";
    for(let char of text) {
        if(char >= 'A' && char <= 'Z')
            char = char.toLowerCase();
        if(!LATIN_TO_BOPOMOFO_PAIRS[char]) {
            _text += char;
            continue;
        }
        _text += LATIN_TO_BOPOMOFO_PAIRS[char];
    }
    return _text;
}