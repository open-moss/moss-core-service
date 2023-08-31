import logger from "./logger.js";
import util from "./util.js";

// 同步创建基础目录
util.createBaseDirsSync();

// 允许无限量的监听器
process.setMaxListeners(Infinity);

/**
 * 未捕获异常事件处理
 */
process.on("uncaughtException", (err, origin) => {
    logger.error(`an unhandled error occurred: ${origin}`, err);
});

/**
 * 未处理的Rejected Promise事件处理
 */
process.on("unhandledRejection", (_, promise) => {
    promise.catch(err => logger.error("an unhandled rejection occurred:", err));
});

/**
 * 警告事件处理
 */
process.on("warning", warning => {
    logger.warn("system warning: ", warning);
});

/**
 * 主动进程退出事件处理
 */
process.on("exit", () => {
    logger.info("service exit");
});

/**
 * kill进程事件处理
 */
process.on("SIGTERM", () => {
    logger.warn("received kill signal");
    process.exit(1);
});

/**
 * Ctrl-C进程事件处理
 */
process.on("SIGINT", () => {
    process.exit(0);
});
