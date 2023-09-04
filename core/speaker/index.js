import path from "path";
import fs from "fs-extra";
import _Speaker from "./lib/Speaker.js";

const PACK_INFO = JSON.parse(fs.readFileSync(path.join(path.resolve(), "package.json")).toString());

export const version = PACK_INFO["version"] || "unknown";
export const Speaker = _Speaker;
