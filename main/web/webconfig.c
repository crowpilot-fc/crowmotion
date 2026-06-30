// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Config web UI: WiFi AP + HTTP server + live WebSocket, with a mode-control
// task that swaps between the radio link (BLE) and config mode (WiFi).

#include "webconfig.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "config.h"
#include "tracker.h"
#include "para_ble.h"

static const char *TAG = "webcfg";

// Embedded single-page app (web/index.html).
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;
static int s_ws_fd = -1;
static volatile bool s_active = false;
static SemaphoreHandle_t s_toggle_sem;

// --- HTTP handlers -----------------------------------------------------------

static esp_err_t h_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start,
                    index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t h_get_config(httpd_req_t *req)
{
    char *json = config_to_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json ? json : "{}");
    free(json);
    return ESP_OK;
}

static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buflen)
{
    int total = req->content_len;
    if (total <= 0 || total >= (int)buflen) {
        return ESP_FAIL;
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0) {
            return ESP_FAIL;
        }
        got += r;
    }
    buf[got] = '\0';
    return ESP_OK;
}

static esp_err_t h_post_config(httpd_req_t *req)
{
    char buf[1024];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_FAIL;
    }
    config_apply_json(buf);
    config_save();
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static esp_err_t h_post_recenter(httpd_req_t *req)
{
    tracker_request_recenter();
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static esp_err_t h_post_autodetect(httpd_req_t *req)
{
    tracker_request_autodetect();
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static esp_err_t h_post_exit(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "ok");
    webconfig_request_toggle();
    return ESP_OK;
}

static esp_err_t h_ws(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        s_ws_fd = httpd_req_to_sockfd(req);  // handshake; remember the client
        ESP_LOGI(TAG, "websocket client connected (fd %d)", s_ws_fd);
        return ESP_OK;
    }
    // Drain any incoming frame (we do not expect commands over WS).
    httpd_ws_frame_t f = {0};
    f.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_recv_frame(req, &f, 0);
    return ESP_OK;
}

// Captive-portal style catch-all: any unknown URL just serves the page.
static esp_err_t h_404(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void httpd_start_all(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 12;
    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }
    httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = h_root},
        {.uri = "/config", .method = HTTP_GET, .handler = h_get_config},
        {.uri = "/config", .method = HTTP_POST, .handler = h_post_config},
        {.uri = "/recenter", .method = HTTP_POST, .handler = h_post_recenter},
        {.uri = "/autodetect", .method = HTTP_POST, .handler = h_post_autodetect},
        {.uri = "/exit", .method = HTTP_POST, .handler = h_post_exit},
        {.uri = "/ws", .method = HTTP_GET, .handler = h_ws, .is_websocket = true},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }
    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, h_404);
}

// --- WiFi AP -----------------------------------------------------------------

static void wifi_ap_start(void)
{
    s_ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap = {0};
    const char *name = config_get()->name;
    int n = snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s",
                     (name[0] ? name : "FreeLook"));
    ap.ap.ssid_len = n;
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "config AP up: SSID \"%s\", http://192.168.4.1", ap.ap.ssid);
}

static void wifi_ap_stop(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
}

// --- Live WebSocket push -----------------------------------------------------

static void ws_push(void)
{
    if (s_ws_fd < 0 || s_server == NULL) {
        return;
    }
    tracker_snapshot_t s;
    tracker_get(&s);
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
                     "{\"y\":%.1f,\"p\":%.1f,\"r\":%.1f,\"pan\":%u,\"tilt\":%u}",
                     s.yaw, s.pitch, s.roll, s.pan_us, s.tilt_us);
    httpd_ws_frame_t f = {0};
    f.type = HTTPD_WS_TYPE_TEXT;
    f.payload = (uint8_t *)buf;
    f.len = n;
    if (httpd_ws_send_frame_async(s_server, s_ws_fd, &f) != ESP_OK) {
        s_ws_fd = -1;  // client gone
    }
}

// --- Mode control ------------------------------------------------------------

static void enter_config(void)
{
    para_ble_pause();
    wifi_ap_start();
    httpd_start_all();
    s_active = true;
}

static void exit_config(void)
{
    s_active = false;
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    s_ws_fd = -1;
    wifi_ap_stop();
    para_ble_resume();
    ESP_LOGI(TAG, "config mode off, radio link resumed");
}

static void mode_task(void *arg)
{
    for (;;) {
        xSemaphoreTake(s_toggle_sem, portMAX_DELAY);
        if (para_ble_is_connected()) {
            ESP_LOGW(TAG, "ignoring config request: radio is connected");
            continue;
        }
        enter_config();
        // Stay in config mode, pushing live data, until another toggle arrives.
        while (s_active) {
            if (xSemaphoreTake(s_toggle_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                break;
            }
            ws_push();
        }
        exit_config();
    }
}

// --- API ---------------------------------------------------------------------

void webconfig_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_toggle_sem = xSemaphoreCreateBinary();
    xTaskCreate(mode_task, "webcfg", 4096, NULL, 5, NULL);
}

void webconfig_request_toggle(void)
{
    if (s_toggle_sem) {
        xSemaphoreGive(s_toggle_sem);
    }
}

bool webconfig_is_active(void)
{
    return s_active;
}
