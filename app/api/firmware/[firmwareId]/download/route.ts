import { NextResponse } from "next/server";
import { firmwareBytes } from "@/lib/firmware";
import { getStore } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function GET(_request: Request, context: { params: Promise<{ firmwareId: string }> }) {
  const { firmwareId } = await context.params;
  const firmware = getStore().firmwareVersions.find((item) => item.id === firmwareId);
  if (!firmware) {
    return NextResponse.json({ error: "firmware not found" }, { status: 404 });
  }

  return new NextResponse(firmwareBytes(firmware.version), {
    headers: {
      "content-type": "application/octet-stream",
      "content-disposition": `attachment; filename="gateway-${firmware.version}.bin"`
    }
  });
}
