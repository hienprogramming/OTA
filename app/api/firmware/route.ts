import { NextRequest, NextResponse } from "next/server";
import { addFirmware, getStore } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function GET() {
  return NextResponse.json({ firmwareVersions: getStore().firmwareVersions });
}

export async function POST(request: NextRequest) {
  const body = await request.json();
  if (!body.version) {
    return NextResponse.json({ error: "version is required" }, { status: 400 });
  }

  const firmware = addFirmware({
    version: String(body.version),
    releaseNotes: body.releaseNotes ? String(body.releaseNotes) : "No release notes."
  });

  return NextResponse.json({ firmware }, { status: 201 });
}
