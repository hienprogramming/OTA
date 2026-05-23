import { NextRequest, NextResponse } from "next/server";
import { upsertVehicle } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function POST(request: NextRequest) {
  const body = await request.json();
  if (!body.vehicleId) {
    return NextResponse.json({ error: "vehicleId is required" }, { status: 400 });
  }

  const vehicle = upsertVehicle({
    vehicleId: String(body.vehicleId),
    vin: body.vin ? String(body.vin) : undefined,
    name: body.name ? String(body.name) : undefined,
    firmwareVersion: body.firmwareVersion ? String(body.firmwareVersion) : undefined
  });

  return NextResponse.json({ vehicle });
}
