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
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

#include "config.h"
#include "tracker.h"
#include "para_ble.h"
#include "version.h"
#include "led.h"

static const char *TAG = "webcfg";

// Embedded single-page app (web/index.html).
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;
static esp_netif_t *s_sta_netif = NULL;
static int s_ws_fd = -1;
static volatile bool s_active = false;
static SemaphoreHandle_t s_toggle_sem;
static EventGroupHandle_t s_wifi_eg;
#define GOTIP_BIT BIT0
static char s_ota_status[72] = "idle";

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_eg, GOTIP_BIT);
        esp_wifi_connect();  // keep retrying the home network
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_eg, GOTIP_BIT);
    }
}

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

// --- OTA ---------------------------------------------------------------------

// Pull update: join home WiFi, fetch the manifest, flash if a newer version.
static void ota_check_task(void *arg)
{
    strcpy(s_ota_status, "connecting to WiFi...");
    EventBits_t b = xEventGroupWaitBits(s_wifi_eg, GOTIP_BIT, false, true,
                                        pdMS_TO_TICKS(20000));
    if (!(b & GOTIP_BIT)) {
        strcpy(s_ota_status, "no internet (check WiFi credentials)");
        vTaskDelete(NULL);
    }
    strcpy(s_ota_status, "checking for updates...");

    char body[512];
    int n = -1;
    esp_http_client_config_t hc = {
        .url = FREELOOK_UPDATE_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t cl = esp_http_client_init(&hc);
    if (esp_http_client_open(cl, 0) == ESP_OK) {
        esp_http_client_fetch_headers(cl);
        n = esp_http_client_read_response(cl, body, sizeof(body) - 1);
    }
    esp_http_client_cleanup(cl);
    if (n <= 0) {
        strcpy(s_ota_status, "could not reach update server");
        vTaskDelete(NULL);
    }
    body[n] = '\0';

    cJSON *j = cJSON_Parse(body);
    cJSON *ver = j ? cJSON_GetObjectItem(j, "version") : NULL;
    cJSON *url = j ? cJSON_GetObjectItem(j, "url") : NULL;
    if (!cJSON_IsString(ver) || !cJSON_IsString(url)) {
        strcpy(s_ota_status, "bad manifest from server");
        if (j) cJSON_Delete(j);
        vTaskDelete(NULL);
    }
    if (strcmp(ver->valuestring, FREELOOK_VERSION) == 0) {
        snprintf(s_ota_status, sizeof(s_ota_status), "up to date (%s)", FREELOOK_VERSION);
        cJSON_Delete(j);
        vTaskDelete(NULL);
    }
    snprintf(s_ota_status, sizeof(s_ota_status), "updating to %s...", ver->valuestring);
    led_set(LED_OTA);
    char ota_url[200];
    strncpy(ota_url, url->valuestring, sizeof(ota_url) - 1);
    ota_url[sizeof(ota_url) - 1] = '\0';
    cJSON_Delete(j);

    esp_http_client_config_t oc = {
        .url = ota_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_cfg = {.http_config = &oc};
    if (esp_https_ota(&ota_cfg) == ESP_OK) {
        strcpy(s_ota_status, "updated, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(800));
        esp_restart();
    } else {
        strcpy(s_ota_status, "update download failed");
    }
    vTaskDelete(NULL);
}

static esp_err_t h_post_checkupdate(httpd_req_t *req)
{
    if (config_get()->wifi_ssid[0] == '\0') {
        httpd_resp_sendstr(req, "set home WiFi first");
        return ESP_OK;
    }
    strcpy(s_ota_status, "starting...");
    xTaskCreate(ota_check_task, "ota", 8192, NULL, 5, NULL);
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static esp_err_t h_get_status(httpd_req_t *req)
{
    httpd_resp_sendstr(req, s_ota_status);
    return ESP_OK;
}

// Local update: the page POSTs a firmware .bin as the body; flash it.
static esp_err_t h_post_update(httpd_req_t *req)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota;
    if (!part || esp_ota_begin(part, OTA_SIZE_UNKNOWN, &ota) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota begin failed");
        return ESP_FAIL;
    }
    led_set(LED_OTA);
    char buf[1024];
    int remaining = req->content_len;
    bool ok = remaining > 0;
    while (remaining > 0) {
        int want = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int r = httpd_req_recv(req, buf, want);
        if (r <= 0 || esp_ota_write(ota, buf, r) != ESP_OK) {
            ok = false;
            break;
        }
        remaining -= r;
    }
    if (!ok) {
        esp_ota_abort(ota);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
        return ESP_FAIL;
    }
    if (esp_ota_end(ota) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "image invalid");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "ok, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
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
    cfg.max_uri_handlers = 16;
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
        {.uri = "/checkupdate", .method = HTTP_POST, .handler = h_post_checkupdate},
        {.uri = "/status", .method = HTTP_GET, .handler = h_get_status},
        {.uri = "/update", .method = HTTP_POST, .handler = h_post_update},
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
    freelook_config_t *cf = config_get();
    bool use_sta = cf->wifi_ssid[0] != '\0';

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (use_sta) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap = {0};
    int n = snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s",
                     (cf->name[0] ? cf->name : "FreeLook"));
    ap.ap.ssid_len = n;
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    if (use_sta) {
        wifi_config_t sta = {0};
        strncpy((char *)sta.sta.ssid, cf->wifi_ssid, sizeof(sta.sta.ssid) - 1);
        strncpy((char *)sta.sta.password, cf->wifi_pass, sizeof(sta.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();
        ESP_LOGI(TAG, "config AP+STA up (joining \"%s\")", cf->wifi_ssid);
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    ESP_LOGI(TAG, "config AP \"%s\" at http://192.168.4.1", ap.ap.ssid);
}

static void wifi_ap_stop(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
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
    led_set(LED_CONFIG);
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
    led_set(LED_SEARCHING);
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
    s_wifi_eg = xEventGroupCreate();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);
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
