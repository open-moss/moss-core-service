import fs from "fs";
import path from "path";
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const speaker = require('./build/Release/speaker');

const PACK_INFO = JSON.parse(fs.readFileSync(path.join(path.resolve(), "package.json")).toString());

console.log(PACK_INFO.version);

(async () => {
    await speaker.loadModel("/home/moss/projects/moss-core-service/models/speaker/moss.onnx", "/home/moss/projects/moss-core-service/models/speaker/moss.json", 4);
    console.log(await speaker.synthesize(new Int16Array([
        0, 47, 0, 11, 0, 28, 0, 47, 0, 49, 0, 21, 0, 38, 0, 47, 0, 32, 0, 47, 0, 1, 0, 49, 0, 26, 0, 42, 0, 39, 0, 45, 0, 49, 0, 7, 0, 42, 0, 45, 0, 23, 0, 48, 0, 49, 0, 24, 0, 42, 0, 29, 0, 47, 0, 20, 0, 41, 0, 34, 0, 46, 0, 1, 0, 49, 0, 40, 0, 45, 0, 23, 0, 47, 0, 49, 0, 34, 0, 47, 0, 9, 0, 36, 0, 47, 0, 2, 0
    ]), 1.0));
    console.log("AAA");
})()
.catch(err => {
    console.error(err);
});
