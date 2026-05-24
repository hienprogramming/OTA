/*
 * Gateway ECU OTA simulator in C.
 *
 * Automotive choice: C is closer to an AUTOSAR Classic style gateway ECU than
 * C++ because it keeps runtime behavior simple and maps cleanly to embedded
 * toolchains. This demo still runs on Windows and uses curl.exe only as the
 * HTTPS transport, while the ECU state machine and SHA-256 verification are C.
 *
 * Build with MinGW:
 *   gcc simulator/gateway_ecu_simulator.c -o gateway_ecu_simulator_c.exe -lm
 *
 * Run:
 *   .\gateway_ecu_simulator_c.exe --cloud-url https://your-project.vercel.app
 *
 * Protected Vercel deployment:
 *   .\gateway_ecu_simulator_c.exe --cloud-url https://your-project.vercel.app --bypass-secret YOUR_SECRET
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_CLOUD_URL "http://localhost:3000"
#define DEFAULT_VEHICLE_ID "VEH-001"
#define DEFAULT_VIN "DEMOAUTOSAR000001"
#define DEFAULT_NAME "Gateway ECU Demo Vehicle"
#define DEFAULT_FIRMWARE_VERSION "1.0.0"
#define DEFAULT_INTERVAL_SECONDS 2.0

typedef struct {
    unsigned char *data;
    size_t size;
} Buffer;

typedef struct {
    char cloud_url[512];
    char vehicle_id[128];
    char vin[128];
    char name[256];
    char firmware_version[64];
    char bypass_secret[512];
    double interval_seconds;
} GatewayConfig;

typedef struct {
    int ticks;
    double odometer_km;
} SensorModel;

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    unsigned char data[64];
    size_t datalen;
} Sha256Context;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32u - bits));
}

static void sha256_transform(Sha256Context *ctx, const unsigned char data[64]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | data[j + 3];
    }
    for (; i < 64; ++i) {
        uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + SHA256_K[i] + m[i];
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(Sha256Context *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u; ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu; ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
}

static void sha256_update(Sha256Context *ctx, const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(Sha256Context *ctx, unsigned char hash[32]) {
    size_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    if (i > 56) {
        while (i < 64) {
            ctx->data[i++] = 0;
        }
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) {
        ctx->data[i++] = 0;
    }

    ctx->bitlen += ctx->datalen * 8;
    for (int shift = 56; shift >= 0; shift -= 8) {
        ctx->data[i++] = (unsigned char)(ctx->bitlen >> shift);
    }
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        for (int j = 0; j < 8; ++j) {
            hash[i + j * 4] = (unsigned char)((ctx->state[j] >> (24 - i * 8)) & 0xff);
        }
    }
}

static void sha256_hex(const unsigned char *data, size_t size, char output[65]) {
    unsigned char hash[32];
    Sha256Context ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, hash);
    for (size_t i = 0; i < sizeof(hash); ++i) {
        snprintf(output + i * 2, 3, "%02x", hash[i]);
    }
    output[64] = '\0';
}

static void buffer_free(Buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

static bool read_file(const char *path, Buffer *buffer) {
    FILE *file = fopen(path, "rb");
    long size;
    if (!file) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    buffer->data = (unsigned char *)malloc((size_t)size + 1);
    if (!buffer->data) {
        fclose(file);
        return false;
    }
    buffer->size = fread(buffer->data, 1, (size_t)size, file);
    buffer->data[buffer->size] = '\0';
    fclose(file);
    return true;
}

static bool write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    fwrite(text, 1, strlen(text), file);
    fclose(file);
    return true;
}

static bool temp_path(char *path, size_t path_size) {
    char directory[MAX_PATH];
    char generated[MAX_PATH];
    if (!GetTempPathA(sizeof(directory), directory)) {
        return false;
    }
    if (!GetTempFileNameA(directory, "ota", 0, generated)) {
        return false;
    }
    snprintf(path, path_size, "%s", generated);
    return true;
}

static void copy_arg(char *destination, size_t destination_size, const char *value) {
    snprintf(destination, destination_size, "%s", value ? value : "");
}

static void strip_trailing_slash(char *value) {
    size_t length = strlen(value);
    while (length > 0 && value[length - 1] == '/') {
        value[--length] = '\0';
    }
}

static bool is_absolute_url(const char *value) {
    return strncmp(value, "http://", 7) == 0 || strncmp(value, "https://", 8) == 0;
}

static bool build_url(const GatewayConfig *config, const char *path_or_url, char *url, size_t url_size) {
    if (is_absolute_url(path_or_url)) {
        snprintf(url, url_size, "%s", path_or_url);
    } else {
        snprintf(url, url_size, "%s%s", config->cloud_url, path_or_url);
    }
    return strlen(url) < url_size - 1;
}

static bool quote_for_cmd(const char *input, char *output, size_t output_size) {
    size_t out = 0;
    if (out + 1 >= output_size) {
        return false;
    }
    output[out++] = '"';
    for (size_t i = 0; input[i] != '\0'; ++i) {
        if (input[i] == '"') {
            if (out + 2 >= output_size) {
                return false;
            }
            output[out++] = '\\';
        }
        if (out + 1 >= output_size) {
            return false;
        }
        output[out++] = input[i];
    }
    if (out + 2 >= output_size) {
        return false;
    }
    output[out++] = '"';
    output[out] = '\0';
    return true;
}

static bool http_request(const GatewayConfig *config, const char *method, const char *path_or_url, const char *json_body, Buffer *response) {
    char url[1024], url_q[1200], response_path[MAX_PATH], response_q[MAX_PATH + 8];
    char body_path[MAX_PATH] = {0}, body_q[MAX_PATH + 8] = {0};
    char command[4096];
    int exit_code;

    memset(response, 0, sizeof(*response));
    if (!build_url(config, path_or_url, url, sizeof(url)) || !temp_path(response_path, sizeof(response_path))) {
        fprintf(stderr, "[gateway] could not prepare HTTP request\n");
        return false;
    }
    if (!quote_for_cmd(url, url_q, sizeof(url_q)) || !quote_for_cmd(response_path, response_q, sizeof(response_q))) {
        return false;
    }

    if (json_body) {
        if (!temp_path(body_path, sizeof(body_path)) || !write_text_file(body_path, json_body) || !quote_for_cmd(body_path, body_q, sizeof(body_q))) {
            fprintf(stderr, "[gateway] could not prepare JSON request body\n");
            return false;
        }
    }

    snprintf(command, sizeof(command), "curl.exe -sS --fail -X %s", method);
    if (config->bypass_secret[0] != '\0') {
        char header[700], header_q[760];
        snprintf(header, sizeof(header), "x-vercel-protection-bypass: %s", config->bypass_secret);
        quote_for_cmd(header, header_q, sizeof(header_q));
        strncat(command, " -H ", sizeof(command) - strlen(command) - 1);
        strncat(command, header_q, sizeof(command) - strlen(command) - 1);
    }
    if (json_body) {
        strncat(command, " -H \"content-type: application/json\" --data-binary @", sizeof(command) - strlen(command) - 1);
        strncat(command, body_q, sizeof(command) - strlen(command) - 1);
    }
    strncat(command, " -o ", sizeof(command) - strlen(command) - 1);
    strncat(command, response_q, sizeof(command) - strlen(command) - 1);
    strncat(command, " ", sizeof(command) - strlen(command) - 1);
    strncat(command, url_q, sizeof(command) - strlen(command) - 1);

    exit_code = system(command);
    if (body_path[0] != '\0') {
        remove(body_path);
    }
    if (exit_code != 0) {
        fprintf(stderr, "[gateway] curl failed for %s\n", url);
        remove(response_path);
        return false;
    }

    bool ok = read_file(response_path, response);
    remove(response_path);
    return ok;
}

static bool url_encode(const char *input, char *output, size_t output_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t written = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)input[i];
        bool safe = isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (safe) {
            if (written + 1 >= output_size) return false;
            output[written++] = (char)ch;
        } else {
            if (written + 3 >= output_size) return false;
            output[written++] = '%';
            output[written++] = hex[ch >> 4];
            output[written++] = hex[ch & 0x0F];
        }
    }
    output[written] = '\0';
    return true;
}

static const char *find_json_key(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *found = strstr(json, pattern);
    if (!found) return NULL;
    found += strlen(pattern);
    while (*found && isspace((unsigned char)*found)) found++;
    if (*found++ != ':') return NULL;
    while (*found && isspace((unsigned char)*found)) found++;
    return found;
}

static bool json_get_string(const char *json, const char *key, char *output, size_t output_size) {
    const char *value = find_json_key(json, key);
    size_t written = 0;
    if (!value || *value != '"') return false;
    value++;
    while (*value && *value != '"') {
        if (*value == '\\' && value[1]) value++;
        if (written + 1 >= output_size) return false;
        output[written++] = *value++;
    }
    output[written] = '\0';
    return *value == '"';
}

static bool json_get_object(const char *json, const char *key, char *output, size_t output_size) {
    const char *value = find_json_key(json, key);
    int depth = 0;
    size_t written = 0;
    bool in_string = false, escape = false;
    if (!value || strncmp(value, "null", 4) == 0 || *value != '{') return false;
    for (const char *cursor = value; *cursor; ++cursor) {
        char ch = *cursor;
        if (written + 1 >= output_size) return false;
        output[written++] = ch;
        if (escape) {
            escape = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escape = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string && ch == '{') depth++;
        if (!in_string && ch == '}' && --depth == 0) {
            output[written] = '\0';
            return true;
        }
    }
    return false;
}

static double random_range(double min_value, double max_value) {
    return min_value + ((double)rand() / (double)RAND_MAX) * (max_value - min_value);
}

static void sensor_sample(SensorModel *model, double *speed, double *rpm, double *battery, double *coolant, double *tire, double *odometer) {
    model->ticks++;
    double phase = (double)model->ticks / 8.0;
    *speed = 62.0 + 28.0 * sin(phase) + random_range(-3.0, 3.0);
    if (*speed < 0.0) *speed = 0.0;
    *rpm = 850.0 + *speed * 38.0 + random_range(-80.0, 80.0);
    if (*rpm < 700.0) *rpm = 700.0;
    *battery = 13.4 + 0.25 * sin(phase / 2.0) + random_range(-0.05, 0.05);
    *coolant = 82.0 + 6.0 * sin(phase / 3.0) + random_range(-1.2, 1.2);
    *tire = 232.0 + 3.0 * sin(phase / 4.0) + random_range(-1.5, 1.5);
    model->odometer_km += *speed / 3600.0;
    *odometer = model->odometer_km;
}

static bool report_status(const GatewayConfig *config, const char *job_id, const char *status, const char *message) {
    char payload[1024];
    Buffer response = {0};
    snprintf(payload, sizeof(payload), "{\"jobId\":\"%s\",\"status\":\"%s\",\"message\":\"%s\"}", job_id, status, message);
    bool ok = http_request(config, "POST", "/api/vehicle/ota/status", payload, &response);
    buffer_free(&response);
    if (ok) printf("[ota] %s: %s\n", status, message);
    return ok;
}

static bool register_vehicle(const GatewayConfig *config) {
    char payload[1024];
    Buffer response = {0};
    snprintf(payload, sizeof(payload), "{\"vehicleId\":\"%s\",\"vin\":\"%s\",\"name\":\"%s\",\"firmwareVersion\":\"%s\"}",
             config->vehicle_id, config->vin, config->name, config->firmware_version);
    bool ok = http_request(config, "POST", "/api/vehicle/register", payload, &response);
    buffer_free(&response);
    if (ok) printf("[gateway] registered %s at %s\n", config->vehicle_id, config->cloud_url);
    return ok;
}

static bool send_telemetry(const GatewayConfig *config, SensorModel *sensor) {
    double speed, rpm, battery, coolant, tire, odometer;
    char payload[1024];
    Buffer response = {0};
    sensor_sample(sensor, &speed, &rpm, &battery, &coolant, &tire, &odometer);
    snprintf(payload, sizeof(payload),
             "{\"vehicleId\":\"%s\",\"speedKph\":%.1f,\"engineRpm\":%.0f,\"batteryVoltage\":%.2f,"
             "\"coolantTempC\":%.1f,\"tirePressureKpa\":%.1f,\"odometerKm\":%.3f}",
             config->vehicle_id, speed, rpm, battery, coolant, tire, odometer);
    bool ok = http_request(config, "POST", "/api/vehicle/telemetry", payload, &response);
    buffer_free(&response);
    if (ok) printf("[telemetry] speed=%.1f km/h rpm=%.0f battery=%.2f V\n", speed, rpm, battery);
    return ok;
}

static bool perform_ota(GatewayConfig *config, const char *job_json, const char *firmware_json) {
    char job_id[128], target_version[64], download_url[512], expected_sha[128], actual_sha[65];
    Buffer firmware = {0};
    bool ok = false;

    if (!json_get_string(job_json, "id", job_id, sizeof(job_id)) ||
        !json_get_string(firmware_json, "version", target_version, sizeof(target_version)) ||
        !json_get_string(firmware_json, "downloadUrl", download_url, sizeof(download_url)) ||
        !json_get_string(firmware_json, "sha256", expected_sha, sizeof(expected_sha))) {
        fprintf(stderr, "[ota] invalid pending OTA payload\n");
        return false;
    }

    printf("[ota] approved job %s: target firmware %s\n", job_id, target_version);
    if (!report_status(config, job_id, "downloading", "Downloading firmware.")) goto fail;
    if (!http_request(config, "GET", download_url, NULL, &firmware)) goto fail;

    if (!report_status(config, job_id, "verifying", "Verifying SHA256 checksum.")) goto fail;
    sha256_hex(firmware.data, firmware.size, actual_sha);
    if (strcmp(actual_sha, expected_sha) != 0) {
        fprintf(stderr, "[ota] checksum mismatch: expected %s, got %s\n", expected_sha, actual_sha);
        goto fail;
    }

    if (!report_status(config, job_id, "installing", "Installing firmware to inactive partition.")) goto fail;
    Sleep(1000);
    if (!report_status(config, job_id, "rebooting", "Rebooting gateway ECU.")) goto fail;
    Sleep(1000);
    copy_arg(config->firmware_version, sizeof(config->firmware_version), target_version);
    if (!report_status(config, job_id, "success", "Gateway ECU is now running target firmware.")) goto fail;
    printf("[ota] job %s completed successfully\n", job_id);
    ok = true;
    goto cleanup;

fail:
    report_status(config, job_id, "failed", "Gateway ECU OTA update failed.");
    printf("[ota] job %s failed\n", job_id);

cleanup:
    buffer_free(&firmware);
    return ok;
}

static bool check_ota(GatewayConfig *config) {
    char encoded_vehicle_id[256], path[512], job_json[4096], firmware_json[4096];
    Buffer response = {0};
    if (!url_encode(config->vehicle_id, encoded_vehicle_id, sizeof(encoded_vehicle_id))) return false;
    snprintf(path, sizeof(path), "/api/vehicle/%s/ota/pending", encoded_vehicle_id);
    if (!http_request(config, "GET", path, NULL, &response)) return false;
    bool has_job = json_get_object((const char *)response.data, "job", job_json, sizeof(job_json));
    bool has_firmware = json_get_object((const char *)response.data, "firmware", firmware_json, sizeof(firmware_json));
    if (has_job && has_firmware) perform_ota(config, job_json, firmware_json);
    buffer_free(&response);
    return true;
}

static void print_usage(const char *program) {
    printf("Usage: %s [options]\n", program);
    printf("  --cloud-url URL              Cloud base URL, default %s\n", DEFAULT_CLOUD_URL);
    printf("  --vehicle-id ID              Vehicle ID, default %s\n", DEFAULT_VEHICLE_ID);
    printf("  --vin VIN                    VIN, default %s\n", DEFAULT_VIN);
    printf("  --name NAME                  Vehicle display name\n");
    printf("  --firmware-version VERSION   Initial firmware version, default %s\n", DEFAULT_FIRMWARE_VERSION);
    printf("  --interval-seconds SECONDS   Loop interval, default %.1f\n", DEFAULT_INTERVAL_SECONDS);
    printf("  --bypass-secret SECRET       Vercel x-vercel-protection-bypass secret\n");
}

static bool parse_args(int argc, char **argv, GatewayConfig *config) {
    copy_arg(config->cloud_url, sizeof(config->cloud_url), DEFAULT_CLOUD_URL);
    copy_arg(config->vehicle_id, sizeof(config->vehicle_id), DEFAULT_VEHICLE_ID);
    copy_arg(config->vin, sizeof(config->vin), DEFAULT_VIN);
    copy_arg(config->name, sizeof(config->name), DEFAULT_NAME);
    copy_arg(config->firmware_version, sizeof(config->firmware_version), DEFAULT_FIRMWARE_VERSION);
    config->bypass_secret[0] = '\0';
    config->interval_seconds = DEFAULT_INTERVAL_SECONDS;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", argv[i]);
            return false;
        }
        if (strcmp(argv[i], "--cloud-url") == 0) copy_arg(config->cloud_url, sizeof(config->cloud_url), argv[++i]);
        else if (strcmp(argv[i], "--vehicle-id") == 0) copy_arg(config->vehicle_id, sizeof(config->vehicle_id), argv[++i]);
        else if (strcmp(argv[i], "--vin") == 0) copy_arg(config->vin, sizeof(config->vin), argv[++i]);
        else if (strcmp(argv[i], "--name") == 0) copy_arg(config->name, sizeof(config->name), argv[++i]);
        else if (strcmp(argv[i], "--firmware-version") == 0) copy_arg(config->firmware_version, sizeof(config->firmware_version), argv[++i]);
        else if (strcmp(argv[i], "--interval-seconds") == 0) config->interval_seconds = atof(argv[++i]);
        else if (strcmp(argv[i], "--bypass-secret") == 0) copy_arg(config->bypass_secret, sizeof(config->bypass_secret), argv[++i]);
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        }
    }
    strip_trailing_slash(config->cloud_url);
    return true;
}

int main(int argc, char **argv) {
    GatewayConfig config;
    SensorModel sensor = {0, 10342.0};
    if (!parse_args(argc, argv, &config)) return 1;
    srand((unsigned int)time(NULL));
    if (!register_vehicle(&config)) return 1;
    for (;;) {
        send_telemetry(&config, &sensor);
        check_ota(&config);
        Sleep((DWORD)(config.interval_seconds * 1000.0));
    }
}
