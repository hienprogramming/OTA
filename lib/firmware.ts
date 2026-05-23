import { createHash } from "crypto";

export function firmwareBytes(version: string): Buffer {
  return Buffer.from(
    [
      "AUTOSAR-OTA-DEMO-FIRMWARE",
      `version=${version}`,
      "target=gateway-ecu",
      "payload=this is a deterministic demo binary"
    ].join("\n"),
    "utf8"
  );
}

export function sha256Hex(data: Buffer | string): string {
  return createHash("sha256").update(data).digest("hex");
}
