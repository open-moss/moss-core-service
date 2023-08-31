import fs from "fs";
import path from "path";

export const LOGO_TEXT = Buffer.from("IF9fX19fICAgICAgICAgICAgIF9fX19fIF9fX19fIF9fX19fIF9fX19fCnwgICAgIHxfX18gX19fIF9fX3wgICAgIHwgICAgIHwgICBfX3wgICBfX3wKfCAgfCAgfCAuIHwgLV98ICAgfCB8IHwgfCAgfCAgfF9fICAgfF9fICAgfAp8X19fX198ICBffF9fX3xffF98X3xffF98X19fX198X19fX198X19fX198CiAgICAgIHxffA==", "base64").toString();
export const BASE_DIR_PATH = path.resolve();
export const PACK_INFO = JSON.parse(fs.readFileSync(path.join(BASE_DIR_PATH, "package.json")).toString());
export const LOG_DIR_PATH = path.join(BASE_DIR_PATH, "logs");
export const TMP_DIR_PATH = path.join(BASE_DIR_PATH, "tmp");
export const GITIGNORE_PATH = path.join(BASE_DIR_PATH, ".gitignore");
