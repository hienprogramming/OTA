import { NextRequest, NextResponse } from "next/server";
import { getStore, updateOtaJob, upsertVehicle } from "@/lib/store";
import type { OtaStatus } from "@/lib/types";

export const dynamic = "force-dynamic";

const allowed: OtaStatus[] = ["downloading", "verifying", "installing", "rebooting", "success", "failed"];

export async function POST(request: NextRequest) {
  const body = await request.json();
  if (!body.jobId || !body.status) {
    return NextResponse.json({ error: "jobId and status are required" }, { status: 400 });
  }

  const status = String(body.status) as OtaStatus;
  if (!allowed.includes(status)) {
    return NextResponse.json({ error: "invalid OTA status" }, { status: 400 });
  }

  const job = updateOtaJob(String(body.jobId), status, body.message ? String(body.message) : status);
  if (!job) {
    return NextResponse.json({ error: "job not found" }, { status: 404 });
  }

  if (status === "success") {
    const store = getStore();
    const firmware = store.firmwareVersions.find((item) => item.id === job.firmwareId);
    if (firmware) {
      upsertVehicle({ vehicleId: job.vehicleId, firmwareVersion: firmware.version });
    }
  }

  return NextResponse.json({ job });
}
