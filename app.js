"use strict"

import "./lib/initialize.js";
import logger from "./lib/logger.js";
import util from "./lib/util.js";
import { Speaker } from "./core/speaker/index.js";
// import "./core/server/index.js";

util.printHello();

logger.info("service name:", util.getServiceName());
logger.debug("process id:", process.pid);
logger.debug("parent process id:", process.ppid);

const speaker = new Speaker({
    modelPath: "/home/moss/projects/moss-core-service/models/speaker/moss.onnx",
    modelConfigPath: "/home/moss/projects/moss-core-service/models/speaker/moss.json",
    numThreads: 4,
    singleSpeaker: true
});

await speaker.setVolume(40);
console.log(await speaker.say("同年十月，行星发动机点燃"));
await new Promise(resolve => setTimeout(resolve, 500));
console.log(await speaker.say("同年十月，行星发动机点燃"));

/**
 * TODO: 服务注册机制
 * MOSS中控服务需通过注册中心发现上位机服务，后续实现。
 */
await new Promise(resolve => setTimeout(resolve, 60000));

// console.log(util.getIPAddress());