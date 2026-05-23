# Vehicle Cloud OTA Demo

Prototype cập nhật firmware cho Vehicle thông qua Cloud, phù hợp deploy lên Vercel.

## Thành phần

- `app/`: Next.js dashboard và API routes.
- `lib/`: OTA data model, firmware hash, in-memory demo store.
- `simulator/gateway_ecu_simulator.py`: Gateway ECU simulator gửi telemetry và xử lý OTA.

## Luồng chạy

1. Gateway ECU simulator gửi sensor telemetry lên `/api/vehicle/telemetry`.
2. Dashboard hiển thị trạng thái vehicle, firmware registry và OTA jobs.
3. Người dùng approve hoặc reject OTA job trên web.
4. Gateway poll `/api/vehicle/[vehicleId]/ota/pending`.
5. Nếu job đã approve, Gateway tải firmware, verify SHA256, giả lập install và báo status.

## Chạy web local

Máy cần có Node.js 20+.

```bash
cd C:\Working\AutoSar\OTA
npm install
npm run dev
```

Mở `http://localhost:3000`.

## Chạy Gateway ECU simulator

```bash to Test
python simulator/gateway_ecu_simulator.py --cloud-url http://localhost:3000
```

Khi deploy lên Vercel:

```bash Real
python simulator/gateway_ecu_simulator.py --cloud-url https://your-project.vercel.app
```

## Deploy Vercel Real

```bash
npm install -> Done



npm run build
vercel deploy
```

## Ghi chú production

Bản demo dùng in-memory store để dễ chạy trên Vercel serverless. Khi làm thật nên thay bằng:

- Vercel Postgres hoặc Neon cho `vehicles`, `telemetry`, `firmware_versions`, `ota_jobs`, `ota_events`.
- Vercel Blob hoặc S3 cho firmware binary.
- Digital signature bằng private key offline và verify bằng public key trong Gateway.
- Device authentication bằng certificate hoặc token riêng cho từng vehicle.
- Rollback policy, A/B partition, battery/network precondition và audit log bất biến.
