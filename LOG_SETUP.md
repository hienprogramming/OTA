# Vercel Logs Setup Guide

Hướng dẫn cấu hình để tự động ghi log từ Vercel deployment vào folder `Log/` mỗi khi deploy.

## Setup Steps

### 1. Tạo Vercel API Token

1. Đăng nhập vào [Vercel Dashboard](https://vercel.com/dashboard)
2. Vào **Settings** → **Tokens**
3. Tạo token mới với quyền **Read-only**
4. Copy token (bạn sẽ chỉ thấy một lần)

### 2. Lấy Vercel Project ID

1. Đăng nhập vào [Vercel Dashboard](https://vercel.com/dashboard)
2. Chọn project **OTA**
3. Vào **Settings** → **General**
4. Tìm **Project ID** và copy nó

### 3. (Optional) Lấy Vercel Team ID

Nếu project là của một team:
1. Vào Vercel Dashboard
2. Chọn team
3. Vào **Settings** → **General**
4. Tìm **Team ID** và copy nó

### 4. Thêm GitHub Secrets

1. Đi tới GitHub repository → **Settings** → **Secrets and variables** → **Actions**
2. Click **New repository secret** và thêm những secrets sau:

| Secret Name | Value |
|---|---|
| `VERCEL_TOKEN` | API Token từ Vercel (bắt buộc) |
| `VERCEL_PROJECT_ID` | Project ID từ Vercel (bắt buộc) |
| `VERCEL_TEAM_ID` | Team ID từ Vercel (tùy chọn, nếu project là của team) |

**Ví dụ:**
- `VERCEL_TOKEN`: `abc123xyz...`
- `VERCEL_PROJECT_ID`: `prj_abc123...`
- `VERCEL_TEAM_ID`: `team_xyz789...` (nếu có)

### 5. Chạy Workflow

**Cách 1: Tự động trigger**
- Workflow sẽ tự động chạy khi push code lên branch `main`
- Logs sẽ được lưu vào folder `Log/` và tự động commit lên repo

**Cách 2: Manual trigger**
1. Đi tới GitHub repository → **Actions**
2. Chọn workflow **Fetch Vercel Logs**
3. Click **Run workflow** → **Run workflow**
4. Workflow sẽ chạy và fetch logs từ Vercel

## Log Files

Logs sẽ được lưu vào folder `Log/` với format:
- `vercel_deployment_YYYYMMDD_HHMMSS.json` - Full deployment data (JSON)
- `vercel_deployment_YYYYMMDD_HHMMSS.txt` - Readable format (Text)

**Ví dụ:**
```
Log/
├── vercel_deployment_20240524_143022.json
├── vercel_deployment_20240524_143022.txt
├── vercel_deployment_20240524_120515.json
└── vercel_deployment_20240524_120515.txt
```

## Sử dụng Script Local

Bạn cũng có thể chạy script trực tiếp trên máy local:

```bash
# Cài dependencies
pip install requests

# Set environment variables (Windows PowerShell)
$env:VERCEL_TOKEN = "your-token-here"
$env:VERCEL_PROJECT_ID = "your-project-id-here"
$env:VERCEL_TEAM_ID = "your-team-id-here"  # optional

# Chạy script
python fetch_vercel_logs.py
```

## Troubleshooting

### "VERCEL_TOKEN not set"
- Kiểm tra GitHub Secrets có được thêm vào không
- Tên secret phải là `VERCEL_TOKEN` (chính xác)

### "VERCEL_PROJECT_ID not set"
- Kiểm tra GitHub Secrets
- Tên secret phải là `VERCEL_PROJECT_ID` (chính xác)

### "No deployments found"
- Kiểm tra project có deployment nào không
- Kiểm tra PROJECT_ID có chính xác không

### Workflow không chạy
- Kiểm tra workflow file `.github/workflows/fetch-vercel-logs.yml` có tồn tại không
- Kiểm tra permission của workflow (Settings → Actions → General)

## References

- [Vercel API Docs](https://vercel.com/docs/api)
- [GitHub Secrets](https://docs.github.com/en/actions/security-guides/using-secrets-in-github-actions)
- [GitHub Actions Workflows](https://docs.github.com/en/actions/using-workflows)
