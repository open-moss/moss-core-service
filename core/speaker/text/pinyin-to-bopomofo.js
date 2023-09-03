// 注音转换表
const BOPOMOFO_REPLACE = [
    [/^m(\d)$/, 'mu$1'],   // 呣
    [/^n(\d)$/, 'N$1'],    // 嗯
    [/^r5$/, 'er5'],       // 〜兒
    [/iu/g, 'iou'],
    [/ui/g, 'uei'],
    [/ong/g, 'ung'],
    [/^yi?/g, 'i'],
    [/^wu?/g, 'u'],
    [/iu/g, 'v'],
    [/^([jqx])u/g, '$1v'],
    [/([iuv])n/g, '$1en'],
    [/^zhi?/g, 'Z'],
    [/^chi?/g, 'C'],
    [/^shi?/g, 'S'],
    [/^([zcsr])i/g, '$1'],
    [/ai/g, 'A'],
    [/ei/g, 'I'],
    [/ao/g, 'O'],
    [/ou/g, 'U'],
    [/ang/g, 'K'],
    [/eng/g, 'G'],
    [/an/g, 'M'],
    [/en/g, 'N'],
    [/er/g, 'R'],
    [/eh/g, 'E'],
    [/([iv])e/g, '$1E'],
    // [/([^0-4])$/g, '$10'],
    [/1$/g, ''],
  ];
  
  const BOPOMOFO_TABLE = {
    'b': 'ㄅ', 'p': 'ㄆ', 'm': 'ㄇ', 'f': 'ㄈ',
    'd': 'ㄉ', 't': 'ㄊ', 'n': 'ㄋ', 'l': 'ㄌ',
    'g': 'ㄍ', 'k': 'ㄎ', 'h': 'ㄏ', 'j': 'ㄐ',
    'q': 'ㄑ', 'x': 'ㄒ', 'Z': 'ㄓ', 'C': 'ㄔ',
    'S': 'ㄕ', 'r': 'ㄖ', 'z': 'ㄗ', 'c': 'ㄘ',
    's': 'ㄙ', 'i': 'ㄧ', 'u': 'ㄨ', 'v': 'ㄩ',
    'a': 'ㄚ', 'o': 'ㄛ', 'e': 'ㄜ', 'E': 'ㄝ',
    'A': 'ㄞ', 'I': 'ㄟ', 'O': 'ㄠ', 'U': 'ㄡ',
    'M': 'ㄢ', 'N': 'ㄣ', 'K': 'ㄤ', 'G': 'ㄥ',
    'R': 'ㄦ', '2': 'ˊ', '3': 'ˇ', '4': 'ˋ',
    '0': '˙', 'ê': 'ㄝ',
  };

export default function(pinyin) {
    pinyin = pinyin.replace(/1/g, '');
    // 查表替换成注音
    for (const [findRe, replace] of BOPOMOFO_REPLACE) {
        pinyin = pinyin.replace(findRe, replace);
    }

    pinyin = Array.from(pinyin)
        .map((char) => BOPOMOFO_TABLE[char] || char)
        .join('');

    return pinyin;
}
