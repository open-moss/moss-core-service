"use strict"

import "./lib/initialize.js";
import logger from "./lib/logger.js";
import util from "./lib/util.js";
import Speaker from "./core/speaker/index.js";
// import "./core/server/index.js";

util.printHello();

logger.info("service name:", util.getServiceName());
logger.debug("process id:", process.pid);
logger.debug("parent process id:", process.ppid);

const speaker = new Speaker({
    modelPath: "/home/moss/projects/moss-core-service/models/speaker/moss.onnx",
    modelConfigPath: "/home/moss/projects/moss-core-service/models/speaker/moss.json",
    numThreads: 4,
    symbols: [
        "_",
        "\uff0c",
        "\u3002",
        "\uff01",
        "\uff1f",
        "\u2014",
        "\u2026",
        "\u3105",
        "\u3106",
        "\u3107",
        "\u3108",
        "\u3109",
        "\u310a",
        "\u310b",
        "\u310c",
        "\u310d",
        "\u310e",
        "\u310f",
        "\u3110",
        "\u3111",
        "\u3112",
        "\u3113",
        "\u3114",
        "\u3115",
        "\u3116",
        "\u3117",
        "\u3118",
        "\u3119",
        "\u311a",
        "\u311b",
        "\u311c",
        "\u311d",
        "\u311e",
        "\u311f",
        "\u3120",
        "\u3121",
        "\u3122",
        "\u3123",
        "\u3124",
        "\u3125",
        "\u3126",
        "\u3127",
        "\u3128",
        "\u3129",
        "\u02c9",
        "\u02ca",
        "\u02c7",
        "\u02cb",
        "\u02d9",
        " "
      ]
});

await speaker.say("你好啊，我是MOSS。");

/**
 * TODO: 服务注册机制
 * MOSS中控服务需通过注册中心发现上位机服务，后续实现。
 */


// console.log(util.getIPAddress());