import { numberToChinese, chineseToBopomofo } from "./mandarin.js";
import latinToBopomofo from "./latin-to-bopomofo.js";

export default function textReplacement(text, cleaner_names) {
    text = numberToChinese(text);
    text = chineseToBopomofo(text);
    text = latinToBopomofo(text);
    console.log(text);
    return text;
}
