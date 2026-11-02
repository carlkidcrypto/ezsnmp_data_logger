"use strict";

const path = require("path");

const releaseDir = path.join(__dirname, "build", "Release");
if (process.platform === "win32") {
  process.env.PATH = `${releaseDir}${path.delimiter}${process.env.PATH || ""}`;
}

module.exports = require(path.join(releaseDir, "ezsnmp_data_logger.node"));
