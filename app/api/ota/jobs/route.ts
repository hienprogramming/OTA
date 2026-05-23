import { NextRequest, NextResponse } from "next/server";
import { addOtaJob, getStore } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function GET() {
  return NextResponse.json({ otaJobs: getStore().otaJobs });
}

export async function POST(request: NextRequest) {
  const body = await request.json();
  if (!body.vehicleId || !body.firmwareId) {
    return NextResponse.json({ error: "vehicleId and firmwareId are required" }, { status: 400 });
  }

  const store = getStore();
  if (!store.vehicles.some((vehicle) => vehicle.id === String(body.vehicleId))) {
    return NextResponse.json({ error: "vehicle not found" }, { status: 404 });
  }
  if (!store.firmwareVersions.some((firmware) => firmware.id === String(body.firmwareId))) {
    return NextResponse.json({ error: "firmware not found" }, { status: 404 });
  }

  const job = addOtaJob({ vehicleId: String(body.vehicleId), firmwareId: String(body.firmwareId) });
  return NextResponse.json({ job }, { status: 201 });
}
