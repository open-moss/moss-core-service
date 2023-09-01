import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const speaker = require('./build/Release/speaker');

console.log(speaker.getVersion());
await speaker.loadModel("/home/moss/projects/moss-core-service/models/speaker/moss.onnx", "/home/moss/projects/moss-core-service/models/speaker/moss.json", 4);
