import { firmwareBytes, sha256Hex } from "./firmware";
import type { FirmwareVersion, OtaEvent, OtaJob, OtaStatus, Store, Telemetry, Vehicle } from "./types";

type GlobalStore = typeof globalThis & { __otaDemoStore?: Store };

const now = () => new Date().toISOString();
const id = (prefix: string) => `${prefix}_${Math.random().toString(36).slice(2, 10)}_${Date.now().toString(36)}`;

function seedStore(): Store {
  const fw100: FirmwareVersion = {
    id: "fw_100",
    version: "1.0.0",
    releaseNotes: "Baseline gateway firmware.",
    sha256: sha256Hex(firmwareBytes("1.0.0")),
    sizeBytes: firmwareBytes("1.0.0").length,
    createdAt: now()
  };
  const fw110: FirmwareVersion = {
    id: "fw_110",
    version: "1.1.0",
    releaseNotes: "Improve OTA state handling and sensor normalization.",
    sha256: sha256Hex(firmwareBytes("1.1.0")),
    sizeBytes: firmwareBytes("1.1.0").length,
    createdAt: now()
  };
  const vehicle: Vehicle = {
    id: "VEH-001",
    vin: "DEMOAUTOSAR000001",
    name: "Gateway ECU Demo Vehicle",
    currentFirmwareVersion: "1.0.0",
    status: "offline",
    lastSeenAt: now()
  };
  const job: OtaJob = {
    id: "job_demo_110",
    vehicleId: vehicle.id,
    firmwareId: fw110.id,
    status: "waiting_user_approval",
    userApproval: "pending",
    createdAt: now(),
    message: "Firmware 1.1.0 is waiting for user confirmation."
  };

  return {
    vehicles: [vehicle],
    telemetry: [],
    firmwareVersions: [fw100, fw110],
    otaJobs: [job],
    otaEvents: [
      {
        id: "evt_demo_110",
        jobId: job.id,
        status: job.status,
        message: job.message ?? "",
        createdAt: now()
      }
    ]
  };
}

export function getStore(): Store {
  const g = globalThis as GlobalStore;
  if (!g.__otaDemoStore) {
    g.__otaDemoStore = seedStore();
  }
  return g.__otaDemoStore;
}

export function upsertVehicle(input: {
  vehicleId: string;
  vin?: string;
  name?: string;
  firmwareVersion?: string;
}): Vehicle {
  const store = getStore();
  const existing = store.vehicles.find((vehicle) => vehicle.id === input.vehicleId);
  if (existing) {
    existing.vin = input.vin ?? existing.vin;
    existing.name = input.name ?? existing.name;
    existing.currentFirmwareVersion = input.firmwareVersion ?? existing.currentFirmwareVersion;
    existing.status = "online";
    existing.lastSeenAt = now();
    return existing;
  }

  const vehicle: Vehicle = {
    id: input.vehicleId,
    vin: input.vin ?? input.vehicleId,
    name: input.name ?? `Vehicle ${input.vehicleId}`,
    currentFirmwareVersion: input.firmwareVersion ?? "1.0.0",
    status: "online",
    lastSeenAt: now()
  };
  store.vehicles.push(vehicle);
  return vehicle;
}

export function addTelemetry(input: Omit<Telemetry, "id" | "createdAt">): Telemetry {
  const store = getStore();
  upsertVehicle({ vehicleId: input.vehicleId });
  const telemetry: Telemetry = { ...input, id: id("tel"), createdAt: now() };
  store.telemetry.unshift(telemetry);
  store.telemetry = store.telemetry.slice(0, 300);
  return telemetry;
}

export function addFirmware(input: { version: string; releaseNotes: string }): FirmwareVersion {
  const store = getStore();
  const bytes = firmwareBytes(input.version);
  const firmware: FirmwareVersion = {
    id: id("fw"),
    version: input.version,
    releaseNotes: input.releaseNotes,
    sha256: sha256Hex(bytes),
    sizeBytes: bytes.length,
    createdAt: now()
  };
  store.firmwareVersions.unshift(firmware);
  return firmware;
}

export function addOtaJob(input: { vehicleId: string; firmwareId: string }): OtaJob {
  const store = getStore();
  const firmware = store.firmwareVersions.find((item) => item.id === input.firmwareId);
  const job: OtaJob = {
    id: id("job"),
    vehicleId: input.vehicleId,
    firmwareId: input.firmwareId,
    status: "waiting_user_approval",
    userApproval: "pending",
    createdAt: now(),
    message: `Firmware ${firmware?.version ?? input.firmwareId} is waiting for user confirmation.`
  };
  store.otaJobs.unshift(job);
  addOtaEvent(job.id, job.status, job.message ?? "");
  return job;
}

export function addOtaEvent(jobId: string, status: OtaStatus, message: string): OtaEvent {
  const store = getStore();
  const event: OtaEvent = { id: id("evt"), jobId, status, message, createdAt: now() };
  store.otaEvents.unshift(event);
  store.otaEvents = store.otaEvents.slice(0, 300);
  return event;
}

export function updateOtaJob(jobId: string, status: OtaStatus, message: string): OtaJob | undefined {
  const store = getStore();
  const job = store.otaJobs.find((item) => item.id === jobId);
  if (!job) return undefined;

  job.status = status;
  job.message = message;
  if (status === "approved") {
    job.userApproval = "approved";
    job.approvedAt = now();
  }
  if (status === "rejected") {
    job.userApproval = "rejected";
    job.completedAt = now();
  }
  if (status === "success" || status === "failed") {
    job.completedAt = now();
  }
  addOtaEvent(jobId, status, message);
  return job;
}
