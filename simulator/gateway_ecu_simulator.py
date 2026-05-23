#!/usr/bin/env python3
"""
Gateway ECU simulator for the Vehicle Cloud OTA demo.

It sends synthetic sensor telemetry, polls the cloud for approved OTA jobs,
downloads deterministic firmware bytes, verifies SHA256, simulates install,
and reports OTA status back to the server.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass


@dataclass
class GatewayConfig:
    cloud_url: str
    vehicle_id: str
    vin: str
    name: str
    firmware_version: str
    interval_seconds: float


class CloudClient:
    def __init__(self, base_url: str) -> None:
        self.base_url = base_url.rstrip("/")

    def post_json(self, path: str, payload: dict) -> dict:
        data = json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            f"{self.base_url}{path}",
            data=data,
            headers={"content-type": "application/json"},
            method="POST",
        )
        return self._json_request(request)

    def get_json(self, path: str) -> dict:
        request = urllib.request.Request(f"{self.base_url}{path}", method="GET")
        return self._json_request(request)

    def download(self, path_or_url: str) -> bytes:
        url = path_or_url if path_or_url.startswith("http") else f"{self.base_url}{path_or_url}"
        request = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(request, timeout=15) as response:
            return response.read()

    @staticmethod
    def _json_request(request: urllib.request.Request) -> dict:
        with urllib.request.urlopen(request, timeout=15) as response:
            return json.loads(response.read().decode("utf-8"))


class SensorModel:
    def __init__(self) -> None:
        self.ticks = 0
        self.odometer_km = 10342.0

    def sample(self, vehicle_id: str) -> dict:
        self.ticks += 1
        phase = self.ticks / 8.0
        speed = max(0.0, 62 + 28 * math.sin(phase) + random.uniform(-3, 3))
        rpm = max(700.0, 850 + speed * 38 + random.uniform(-80, 80))
        battery = 13.4 + 0.25 * math.sin(phase / 2) + random.uniform(-0.05, 0.05)
        coolant = 82 + 6 * math.sin(phase / 3) + random.uniform(-1.2, 1.2)
        tire = 232 + 3 * math.sin(phase / 4) + random.uniform(-1.5, 1.5)
        self.odometer_km += speed / 3600.0
        return {
            "vehicleId": vehicle_id,
            "speedKph": round(speed, 1),
            "engineRpm": round(rpm, 0),
            "batteryVoltage": round(battery, 2),
            "coolantTempC": round(coolant, 1),
            "tirePressureKpa": round(tire, 1),
            "odometerKm": round(self.odometer_km, 3),
        }


class GatewayEcu:
    def __init__(self, config: GatewayConfig) -> None:
        self.config = config
        self.client = CloudClient(config.cloud_url)
        self.sensors = SensorModel()
        self.completed_jobs: set[str] = set()

    def register(self) -> None:
        payload = {
            "vehicleId": self.config.vehicle_id,
            "vin": self.config.vin,
            "name": self.config.name,
            "firmwareVersion": self.config.firmware_version,
        }
        self.client.post_json("/api/vehicle/register", payload)
        print(f"[gateway] registered {self.config.vehicle_id} at {self.config.cloud_url}")

    def run_forever(self) -> None:
        self.register()
        while True:
            try:
                self.send_telemetry()
                self.check_ota()
            except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
                print(f"[gateway] cloud communication error: {exc}")
            time.sleep(self.config.interval_seconds)

    def send_telemetry(self) -> None:
        telemetry = self.sensors.sample(self.config.vehicle_id)
        self.client.post_json("/api/vehicle/telemetry", telemetry)
        print(
            "[telemetry] "
            f"speed={telemetry['speedKph']} km/h "
            f"rpm={telemetry['engineRpm']} "
            f"battery={telemetry['batteryVoltage']} V"
        )

    def check_ota(self) -> None:
        vehicle_id = urllib.parse.quote(self.config.vehicle_id)
        response = self.client.get_json(f"/api/vehicle/{vehicle_id}/ota/pending")
        job = response.get("job")
        firmware = response.get("firmware")
        if not job or not firmware or job["id"] in self.completed_jobs:
            return
        self.perform_ota(job, firmware)

    def perform_ota(self, job: dict, firmware: dict) -> None:
        job_id = job["id"]
        target_version = firmware["version"]
        print(f"[ota] approved job {job_id}: target firmware {target_version}")

        try:
            self.report(job_id, "downloading", f"Downloading firmware {target_version}.")
            firmware_bytes = self.client.download(firmware["downloadUrl"])

            self.report(job_id, "verifying", "Verifying SHA256 checksum.")
            digest = hashlib.sha256(firmware_bytes).hexdigest()
            if digest != firmware["sha256"]:
                raise ValueError(f"checksum mismatch: expected {firmware['sha256']}, got {digest}")

            self.report(job_id, "installing", "Installing firmware to inactive partition.")
            time.sleep(1.0)
            self.report(job_id, "rebooting", "Rebooting gateway ECU.")
            time.sleep(1.0)

            self.config.firmware_version = target_version
            self.report(job_id, "success", f"Gateway ECU is now running firmware {target_version}.")
            self.completed_jobs.add(job_id)
            print(f"[ota] job {job_id} completed successfully")
        except Exception as exc:
            self.report(job_id, "failed", str(exc))
            self.completed_jobs.add(job_id)
            print(f"[ota] job {job_id} failed: {exc}")

    def report(self, job_id: str, status: str, message: str) -> None:
        self.client.post_json(
            "/api/vehicle/ota/status",
            {"jobId": job_id, "status": status, "message": message},
        )
        print(f"[ota] {status}: {message}")


def parse_args() -> GatewayConfig:
    parser = argparse.ArgumentParser(description="Gateway ECU OTA simulator")
    parser.add_argument("--cloud-url", default="http://localhost:3000")
    parser.add_argument("--vehicle-id", default="VEH-001")
    parser.add_argument("--vin", default="DEMOAUTOSAR000001")
    parser.add_argument("--name", default="Gateway ECU Demo Vehicle")
    parser.add_argument("--firmware-version", default="1.0.0")
    parser.add_argument("--interval-seconds", type=float, default=2.0)
    args = parser.parse_args()
    return GatewayConfig(
        cloud_url=args.cloud_url,
        vehicle_id=args.vehicle_id,
        vin=args.vin,
        name=args.name,
        firmware_version=args.firmware_version,
        interval_seconds=args.interval_seconds,
    )


if __name__ == "__main__":
    GatewayEcu(parse_args()).run_forever()
