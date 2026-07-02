// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// FrSky PARA Bluetooth wireless trainer link (Milestone 1).
//
// Original implementation on ESP-IDF + NimBLE. The PARA link is a wire
// protocol; the framing here is reimplemented from its documented behavior
// (OpenTX trainer-over-Bluetooth) and verified against the public-domain
// ysoldak/HeadTracker. No third-party source code is reused.
//
// Behavior:
//   - Advertise a primary GATT service 0xFFF0 with characteristic 0xFFF6.
//   - When the radio (FrSky X20S / PARA) connects, wait ~1s, send a one-shot
//     "\r\n" nudge so the radio's OpenTX side latches "Connected" and enters
//     trainer-receive, then stream channel frames at 50 Hz.
//   - The radio never subscribes via CCCD, so notifications are forced with
//     ble_gatts_notify_custom(), which sends regardless of subscription.
//   - Until the IMU pipeline is online (M2+), all channels are held at center,
//     so the radio should show eight centered trainer channels.

#include "para_ble.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "led.h"

static const char *TAG = "para";

// --- PARA wire protocol constants ---
#define PARA_START_STOP 0x7E
#define PARA_BYTE_STUFF 0x7D
#define PARA_STUFF_MASK 0x20
#define PARA_FRAME_TYPE 0x80

// Worst-case frame: start(1) + type(stuffed 2) + 12 data bytes(all stuffed 24)
// + crc(1) + end(1) = 29. Round up.
#define PARA_FRAME_MAX 32

// Streaming cadence and the post-connect settle delay before the "\r\n" nudge.
#define PARA_TX_PERIOD_MS 20
#define PARA_CONNECT_SETTLE_MS 1000

#define PARA_DEVICE_NAME "CrowMotion"

// --- BLE service / characteristic UUIDs (16-bit FrSky PARA) ---
#define PARA_SVC_UUID 0xFFF0
#define PARA_SVC_SETTINGS_UUID 0xFFFA  // advertised alongside 0xFFF0, as FrSky expects
#define PARA_CHR_FFF6_UUID 0xFFF6

// Current channel values, centered until the IMU pipeline drives them.
static uint16_t s_channels[CROWMOTION_PARA_NUM_CHANNELS];

// BLE state.
static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_fff6_val_handle;
static bool s_boot_sent;
static TickType_t s_stream_at_tick;
static bool s_paused = false;   // true while WiFi config mode owns the radio

static void para_advertise(void);

// --- Channel state -----------------------------------------------------------

static void channels_init_centered(void)
{
    for (int i = 0; i < CROWMOTION_PARA_NUM_CHANNELS; i++) {
        s_channels[i] = CROWMOTION_PARA_CH_CENTER;
    }
}

void para_ble_set_channel(uint8_t ch, uint16_t value_us)
{
    if (ch >= CROWMOTION_PARA_NUM_CHANNELS) {
        return;
    }
    if (value_us < CROWMOTION_PARA_CH_MIN) {
        value_us = CROWMOTION_PARA_CH_MIN;
    } else if (value_us > CROWMOTION_PARA_CH_MAX) {
        value_us = CROWMOTION_PARA_CH_MAX;
    }
    s_channels[ch] = value_us;
}

uint16_t para_ble_get_channel(uint8_t ch)
{
    return (ch < CROWMOTION_PARA_NUM_CHANNELS) ? s_channels[ch] : 0;
}

// --- PARA frame encoding -----------------------------------------------------

// Append one byte: update XOR CRC, byte-stuff START_STOP/BYTE_STUFF values.
// Worst case frame (all bytes stuffed) is 1 + 2 + 12*2 + 1 + 1 = 29 bytes,
// which fits PARA_FRAME_MAX (32); the bound check is a guard against future
// edits growing the payload past the buffer.
static void para_push(uint8_t *buf, uint8_t *idx, uint8_t *crc, uint8_t b)
{
    if (*idx >= PARA_FRAME_MAX - 1) {
        return;  // never overrun the frame buffer
    }
    *crc ^= b;
    if (b == PARA_START_STOP || b == PARA_BYTE_STUFF) {
        buf[(*idx)++] = PARA_BYTE_STUFF;
        b ^= PARA_STUFF_MASK;
    }
    buf[(*idx)++] = b;
}

// Encode the eight channels into a PARA trainer frame. Returns frame length.
// Two 12-bit channels are packed into three bytes. The leading/trailing
// START_STOP and the CRC byte are written raw (not stuffed), matching the
// FrSky PARA framing.
static uint8_t para_encode_frame(uint8_t *buf)
{
    uint8_t idx = 0;
    uint8_t crc = 0;

    buf[idx++] = PARA_START_STOP;
    para_push(buf, &idx, &crc, PARA_FRAME_TYPE);

    for (int ch = 0; ch < CROWMOTION_PARA_NUM_CHANNELS; ch += 2) {
        uint16_t v1 = s_channels[ch];
        uint16_t v2 = s_channels[ch + 1];
        para_push(buf, &idx, &crc, v1 & 0x00FF);
        para_push(buf, &idx, &crc, ((v1 & 0x0F00) >> 4) | ((v2 & 0x00F0) >> 4));
        para_push(buf, &idx, &crc, ((v2 & 0x000F) << 4) | ((v2 & 0x0F00) >> 8));
    }

    buf[idx++] = crc;
    buf[idx++] = PARA_START_STOP;
    return idx;
}

// Force a notification on 0xFFF6 regardless of CCCD subscription state.
static int para_notify(const uint8_t *data, uint16_t len)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return BLE_HS_ENOTCONN;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return BLE_HS_ENOMEM;
    }
    return ble_gatts_notify_custom(s_conn_handle, s_fff6_val_handle, om);
}

// --- TX task: nudge then stream frames ---------------------------------------

static void para_tx_task(void *arg)
{
    static const uint8_t boot_nudge[2] = {0x0D, 0x0A};  // "\r\n"
    uint8_t frame[PARA_FRAME_MAX];

    for (;;) {
        if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE &&
            (int32_t)(xTaskGetTickCount() - s_stream_at_tick) >= 0) {
            if (!s_boot_sent) {
                if (para_notify(boot_nudge, sizeof(boot_nudge)) == 0) {
                    s_boot_sent = true;
                    ESP_LOGI(TAG, "Sent trainer-receive nudge, streaming channels");
                }
            } else {
                uint8_t len = para_encode_frame(frame);
                para_notify(frame, len);  // best-effort; drops if link is busy
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PARA_TX_PERIOD_MS));
    }
}

// --- GATT service definition -------------------------------------------------

static int fff6_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // The radio writes without response and we push via notify, so reads/writes
    // here are not part of the data path. Accept them quietly.
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(PARA_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(PARA_CHR_FFF6_UUID),
                .access_cb = fff6_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP |
                         BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_fff6_val_handle,
            },
            {0},
        },
    },
    {0},
};

// --- GAP ---------------------------------------------------------------------

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_boot_sent = false;
            s_stream_at_tick = xTaskGetTickCount() + pdMS_TO_TICKS(PARA_CONNECT_SETTLE_MS);
            led_set(LED_CONNECTED);
            ESP_LOGI(TAG, "Radio connected (handle %d), settling before stream",
                     s_conn_handle);
        } else {
            ESP_LOGW(TAG, "Connect failed (status %d), re-advertising",
                     event->connect.status);
            para_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Radio disconnected (reason %d), re-advertising",
                 event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_boot_sent = false;
        led_set(LED_SEARCHING);
        para_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        para_advertise();
        return 0;

    default:
        return 0;
    }
}

bool para_ble_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

void para_ble_pause(void)
{
    s_paused = true;
    ble_gap_adv_stop();
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

void para_ble_resume(void)
{
    s_paused = false;
    para_advertise();
}

static void para_advertise(void)
{
    if (s_paused) {
        return;
    }
    const char *name = ble_svc_gap_device_name();

    // Advertising packet: flags + the FrSky service UUIDs (0xFFF0, 0xFFFA).
    // The name goes in the scan response, not here: FrSky's trainer scan reads
    // the display name from the scan response, so without it the radio shows
    // the device's address instead of "CrowMotion".
    struct ble_hs_adv_fields adv = {0};
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    ble_uuid16_t uuids[] = {
        BLE_UUID16_INIT(PARA_SVC_UUID),
        BLE_UUID16_INIT(PARA_SVC_SETTINGS_UUID),
    };
    adv.uuids16 = uuids;
    adv.num_uuids16 = 2;
    adv.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&adv);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed (rc %d)", rc);
        return;
    }

    // Scan response: the complete local name.
    struct ble_hs_adv_fields rsp = {0};
    rsp.name = (uint8_t *)name;
    rsp.name_len = strlen(name);
    rsp.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields failed (rc %d)", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed (rc %d)", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising as \"%s\" (services 0x%04X, 0x%04X)", name,
             PARA_SVC_UUID, PARA_SVC_SETTINGS_UUID);
}

// --- Host stack lifecycle ----------------------------------------------------

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr failed (rc %d)", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer_auto failed (rc %d)", rc);
        return;
    }
    para_advertise();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset (reason %d)", reason);
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

static void host_task(void *param)
{
    nimble_port_run();  // returns only at nimble_port_stop()
    nimble_port_freertos_deinit();
}

int para_ble_start(void)
{
    channels_init_centered();

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed (%d)", err);
        return -1;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_count_cfg failed (rc %d)", rc);
        return -1;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_add_svcs failed (rc %d)", rc);
        return -1;
    }

    rc = ble_svc_gap_device_name_set(PARA_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "device_name_set failed (rc %d)", rc);
        return -1;
    }

    nimble_port_freertos_init(host_task);

    BaseType_t ok = xTaskCreate(para_tx_task, "para_tx", 3072, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create para_tx task");
        return -1;
    }

    ESP_LOGI(TAG, "PARA link up: %d channels at %d us, advertising as \"%s\"",
             CROWMOTION_PARA_NUM_CHANNELS, CROWMOTION_PARA_CH_CENTER, PARA_DEVICE_NAME);
    return 0;
}
