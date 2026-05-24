"use client";

import { useEffect, useMemo, useState } from "react";
import type { FormEvent, ReactNode } from "react";
import type { FirmwareVersion, OtaEvent, OtaJob, Store, Telemetry, Vehicle } from "@/lib/types";

type ApiState = Store;

const emptyState: ApiState = {
  vehicles: [],
  telemetry: [],
  firmwareVersions: [],
  otaJobs: [],
  otaEvents: []
};

async function postJson(path: string, body?: unknown) {
  const response = await fetch(path, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: body ? JSON.stringify(body) : undefined
  });
  if (!response.ok) {
    const payload = await response.json().catch(() => ({}));
    throw new Error(payload.error ?? `Request failed: ${response.status}`);
  }
  return response.json();
}

function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : String(error);
}

function statusClass(status: string) {
  if (["success", "approved", "online"].includes(status)) return "good";
  if (["failed", "rejected", "offline"].includes(status)) return "bad";
  return "warn";
}

function latestTelemetry(vehicle: Vehicle, telemetry: Telemetry[]) {
  return telemetry.find((item) => item.vehicleId === vehicle.id);
}

export default function Dashboard() {
  const [state, setState] = useState<ApiState>(emptyState);
  const [error, setError] = useState("");
  const [version, setVersion] = useState("1.2.0");
  const [releaseNotes, setReleaseNotes] = useState("Demo OTA package generated from dashboard.");
  const [selectedVehicle, setSelectedVehicle] = useState("VEH-001");
  const [selectedFirmware, setSelectedFirmware] = useState("fw_110");

  const refresh = async () => {
    const response = await fetch("/api/state", { cache: "no-store" });
    const payload = await response.json();
    setState(payload);
    setSelectedVehicle((current) => current || payload.vehicles[0]?.id || "");
    setSelectedFirmware((current) => current || payload.firmwareVersions[0]?.id || "");
  };

  useEffect(() => {
    refresh().catch((err) => setError(errorMessage(err)));
    const timer = window.setInterval(() => {
      refresh().catch((err) => setError(errorMessage(err)));
    }, 2500);
    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!state.vehicles.some((vehicle) => vehicle.id === selectedVehicle)) {
      setSelectedVehicle(state.vehicles[0]?.id ?? "");
    }
    if (!state.firmwareVersions.some((firmware) => firmware.id === selectedFirmware)) {
      setSelectedFirmware(state.firmwareVersions[0]?.id ?? "");
    }
  }, [state, selectedVehicle, selectedFirmware]);

  const waitingJobs = useMemo(
    () => state.otaJobs.filter((job) => job.userApproval === "pending"),
    [state.otaJobs]
  );

  const firmwareById = useMemo(() => {
    return new Map(state.firmwareVersions.map((firmware) => [firmware.id, firmware]));
  }, [state.firmwareVersions]);

  const createFirmware = async (event: FormEvent) => {
    event.preventDefault();
    setError("");
    await postJson("/api/firmware", { version, releaseNotes });
    await refresh();
  };

  const createJob = async (event: FormEvent) => {
    event.preventDefault();
    setError("");
    await postJson("/api/ota/jobs", { vehicleId: selectedVehicle, firmwareId: selectedFirmware });
    await refresh();
  };

  const approve = async (jobId: string) => {
    setError("");
    await postJson(`/api/ota/jobs/${jobId}/approve`);
    await refresh();
  };

  const reject = async (jobId: string) => {
    setError("");
    await postJson(`/api/ota/jobs/${jobId}/reject`);
    await refresh();
  };

  return (
    <main>
      <header className="topbar">
        <div>
          <p className="eyebrow">Vehicle to Cloud OTA</p>
          <h1>Gateway ECU Firmware Update Console</h1>
        </div>
        <div className="summary">
          <span>{state.vehicles.length} vehicles</span>
          <span>{state.otaJobs.length} OTA jobs</span>
          <span>{state.telemetry.length} telemetry frames</span>
        </div>
      </header>

      {error && <section className="error">{error}</section>}

      <section className="approvalBand">
        <div>
          <p className="eyebrow">User confirmation</p>
          <h2>Firmware updates waiting for approval</h2>
        </div>
        <div className="approvalList">
          {waitingJobs.length === 0 && <span className="muted">No pending OTA approval.</span>}
          {waitingJobs.map((job) => {
            const firmware = firmwareById.get(job.firmwareId);
            return (
              <article className="approvalItem" key={job.id}>
                <div>
                  <strong>{job.vehicleId}</strong>
                  <span>Firmware {firmware?.version ?? job.firmwareId}</span>
                </div>
                <div className="actions">
                  <button className="secondary" onClick={() => reject(job.id)}>Reject</button>
                  <button onClick={() => approve(job.id)}>Approve</button>
                </div>
              </article>
            );
          })}
        </div>
      </section>

      <section className="grid">
        <Panel title="Vehicles">
          <div className="table">
            {state.vehicles.map((vehicle) => {
              const telemetry = latestTelemetry(vehicle, state.telemetry);
              return (
                <div className="row" key={vehicle.id}>
                  <div>
                    <strong>{vehicle.id}</strong>
                    <span>{vehicle.name}</span>
                  </div>
                  <div>
                    <span className={`pill ${statusClass(vehicle.status)}`}>{vehicle.status}</span>
                    <span>FW {vehicle.currentFirmwareVersion}</span>
                  </div>
                  <div className="sensorLine">
                    <span>{telemetry ? `${telemetry.speedKph.toFixed(0)} km/h` : "-- km/h"}</span>
                    <span>{telemetry ? `${telemetry.engineRpm.toFixed(0)} rpm` : "-- rpm"}</span>
                    <span>{telemetry ? `${telemetry.batteryVoltage.toFixed(1)} V` : "-- V"}</span>
                  </div>
                </div>
              );
            })}
          </div>
        </Panel>

        <Panel title="Create OTA Job">
          <form onSubmit={createJob} className="form">
            <label>
              Vehicle
              <select value={selectedVehicle} onChange={(event) => setSelectedVehicle(event.target.value)}>
                {state.vehicles.map((vehicle) => (
                  <option key={vehicle.id} value={vehicle.id}>{vehicle.id}</option>
                ))}
              </select>
            </label>
            <label>
              Firmware
              <select value={selectedFirmware} onChange={(event) => setSelectedFirmware(event.target.value)}>
                {state.firmwareVersions.map((firmware) => (
                  <option key={firmware.id} value={firmware.id}>{firmware.version}</option>
                ))}
              </select>
            </label>
            <button disabled={!selectedVehicle || !selectedFirmware}>Create job</button>
          </form>
        </Panel>

        <Panel title="Firmware Registry">
          <form onSubmit={createFirmware} className="form compact">
            <label>
              Version
              <input value={version} onChange={(event) => setVersion(event.target.value)} />
            </label>
            <label>
              Release notes
              <textarea value={releaseNotes} onChange={(event) => setReleaseNotes(event.target.value)} />
            </label>
            <button>Create firmware</button>
          </form>
          <div className="list">
            {state.firmwareVersions.map((firmware) => (
              <FirmwareItem key={firmware.id} firmware={firmware} />
            ))}
          </div>
        </Panel>

        <Panel title="OTA Jobs">
          <div className="list">
            {state.otaJobs.map((job) => (
              <JobItem key={job.id} job={job} firmware={firmwareById.get(job.firmwareId)} />
            ))}
          </div>
        </Panel>

        <Panel title="OTA Event Log">
          <div className="eventLog">
            {state.otaEvents.map((event) => (
              <EventItem key={event.id} event={event} />
            ))}
          </div>
        </Panel>
      </section>
    </main>
  );
}

function Panel({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="panel">
      <h2>{title}</h2>
      {children}
    </section>
  );
}

function FirmwareItem({ firmware }: { firmware: FirmwareVersion }) {
  return (
    <article className="item">
      <div>
        <strong>{firmware.version}</strong>
        <span>{firmware.releaseNotes}</span>
      </div>
      <code>{firmware.sha256.slice(0, 16)}...</code>
    </article>
  );
}

function JobItem({ job, firmware }: { job: OtaJob; firmware?: FirmwareVersion }) {
  return (
    <article className="item">
      <div>
        <strong>{job.vehicleId} → {firmware?.version ?? job.firmwareId}</strong>
        <span>{job.message ?? "No message"}</span>
      </div>
      <span className={`pill ${statusClass(job.status)}`}>{job.status}</span>
    </article>
  );
}

function EventItem({ event }: { event: OtaEvent }) {
  return (
    <article className="event">
      <span className={`dot ${statusClass(event.status)}`} />
      <div>
        <strong>{event.status}</strong>
        <span>{event.message}</span>
      </div>
      <time>{new Date(event.createdAt).toLocaleTimeString()}</time>
    </article>
  );
}
