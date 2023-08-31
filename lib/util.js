import fs from "fs-extra";
import logger from "./logger.js";
import address from "address";
import {
    LOGO_TEXT,
    PACK_INFO,
    LOG_DIR_PATH,
    TMP_DIR_PATH,
    GITIGNORE_PATH
} from "./consts.js";

export default {

    printHello() {
        console.log(`\x1b[36\m${LOGO_TEXT}\n\x1b[0m`);
        console.log("\u{1F680} MOSS上位机服务启动中...");
        console.log(`\u{1F6A9} 服务版本：${this.getServiceVersion()}`);
        console.log(`\u{1F6E0}  当前环境：development`);
        console.log(`\u{1F4D7} 使用文档：https://github.com/open-moss/moss-core-service`);
        console.log(`\u{1F517} 参与开源：https://github.com/open-moss`);
        console.log();
    },

    getServiceName() {
        return PACK_INFO["name"];
    },

    getServiceVersion() {
        return PACK_INFO["version"];
    },

    createBaseDirsSync() {
        [
            LOG_DIR_PATH,
            TMP_DIR_PATH
        ].map(p => fs.ensureDirSync(p));
    },

    getIPAddress() {
        return address.ip();
    }

};