import { NextResponse } from "next/server";
import { updateOtaJob } from "@/lib/store";

export const dynamic = "force-dynamic";

export async function POST(_request: Request, context: { params: Promise<{ jobId: string }> }) {
  const { jobId } = await context.params;
  const job = updateOtaJob(jobId, "rejected", "User rejected OTA update.");
  if (!job) {
    return NextResponse.json({ error: "job not found" }, { status: 404 });
  }
  return NextResponse.json({ job });
}
