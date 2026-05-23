export type VehicleStatus = "online" | "offline";

export type OtaStatus =
  | "created"
  | "waiting_user_approval"
  | "approved"
  | "rejected"
  | "downloading"
  | "verifying"
  | "installing"
  | "rebooting"
  | "success"
  | "failed";

export type Vehicle = {
  id: string;
  vin: string;
  name: string;
  currentFirmwareVersion: string;
  status: VehicleStatus;
  lastSeenAt: string;
};

export type Telemetry = {
  id: string;
  vehicleId: string;
  speedKph: number;
  engineRpm: number;
  batteryVoltage: number;
  coolantTempC: number;
  tirePressureKpa: number;
  odometerKm: number;
  createdAt: string;
};

export type FirmwareVersion = {
  id: string;
  version: string;
  releaseNotes: string;
  sha256: string;
  sizeBytes: number;
  createdAt: string;
};

export type OtaJob = {
  id: string;
  vehicleId: string;
  firmwareId: string;
  status: OtaStatus;
  userApproval: "pending" | "approved" | "rejected";
  createdAt: string;
  approvedAt?: string;
  completedAt?: string;
  message?: string;
};

export type OtaEvent = {
  id: string;
  jobId: string;
  status: OtaStatus;
  message: string;
  createdAt: string;
};

export type Store = {
  vehicles: Vehicle[];
  telemetry: Telemetry[];
  firmwareVersions: FirmwareVersion[];
  otaJobs: OtaJob[];
  otaEvents: OtaEvent[];
};
