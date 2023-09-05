import { numberToChinese, chineseToBopomofo } from "./mandarin.js";
import latinToBopomofo from "./latin-to-bopomofo.js";

export default {
    chinese_cleaners(text) {
        text = numberToChinese(text);
        text = chineseToBopomofo(text);
        text = latinToBopomofo(text);
        console.log(text);
        return text;
    }
};