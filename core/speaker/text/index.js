import { chineseToBopomofo, latinToBopmofo } from "./mandarin.js";

export default function textReplacement(text, cleaner_names) {
    text = chineseToBopomofo(text);
    text = latinToBopmofo(text);
    console.log(text);
    return text;
}
