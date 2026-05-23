import { NextRequest, NextResponse } from "next/server";
import { addTelemetry } from "@/lib/store";

export const dynamic = "force-dynamic";

const numberOr = (value: unknown, fallback: number) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
};

export async function POST(request: NextRequest) {
  const body = await request.json();
  if (!body.vehicleId) {
    return NextResponse.json({ error: "vehicleId is required" }, { status: 400 });
  }

  const telemetry = addTelemetry({
    vehicleId: String(body.vehicleId),
    speedKph: numberOr(body.speedKph, 0),
    engineRpm: numberOr(body.engineRpm, 0),
    batteryVoltage: numberOr(body.batteryVoltage, 12),
    coolantTempC: numberOr(body.coolantTempC, 75),
    tirePressureKpa: numberOr(body.tirePressureKpa, 230),
    odometerKm: numberOr(body.odometerKm, 0)
  });

  return NextResponse.json({ telemetry });
}
