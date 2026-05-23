import { NextResponse } from "next/server";
import { getStore, upsertVehicle } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function GET(_request: Request, context: { params: Promise<{ vehicleId: string }> }) {
  const { vehicleId } = await context.params;
  const store = getStore();
  upsertVehicle({ vehicleId });

  const job = store.otaJobs.find(
    (item) => item.vehicleId === vehicleId && item.userApproval === "approved" && !["success", "failed"].includes(item.status)
  );

  if (!job) {
    return NextResponse.json({ job: null });
  }

  const firmware = store.firmwareVersions.find((item) => item.id === job.firmwareId);
  if (!firmware) {
    return NextResponse.json({ error: "firmware not found" }, { status: 404 });
  }

  return NextResponse.json({
    job,
    firmware: {
      ...firmware,
      downloadUrl: `/api/firmware/${firmware.id}/download`
    }
  });
}
