// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowLink - wireless trainer bridge for CrowMotion
// PPM trainer-signal generator (see ppm.h).

#include "ppm.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"

static const char *TAG = "ppm";

#define PPM_RESOLUTION_HZ 1000000  // 1 us per RMT tick

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;

void ppm_init(void)
{
    rmt_tx_channel_config_t cc = {
        .gpio_num = PPM_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = PPM_RESOLUTION_HZ,
        .mem_block_symbols = 48,
        .trans_queue_depth = 2,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&cc, &s_chan));

    rmt_copy_encoder_config_t ec = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&ec, &s_encoder));

    ESP_ERROR_CHECK(rmt_enable(s_chan));
    ESP_LOGI(TAG, "PPM out on GPIO%d (%d ch, %d us frame)", PPM_GPIO,
             PPM_NUM_CH, PPM_FRAME_US);
}

void ppm_send_frame(const uint16_t ch_us[PPM_NUM_CH])
{
    // One RMT symbol per channel: 300 us high, then low for the remainder
    // of the channel time. A final symbol carries the end pulse and the
    // sync gap that pads the frame to PPM_FRAME_US.
    rmt_symbol_word_t sym[PPM_NUM_CH + 1];
    uint32_t used = 0;

    for (int i = 0; i < PPM_NUM_CH; i++) {
        uint16_t v = ch_us[i];
        if (v < 988) v = 988;
        if (v > 2012) v = 2012;
        sym[i].level0 = 1;
        sym[i].duration0 = PPM_PULSE_US;
        sym[i].level1 = 0;
        sym[i].duration1 = v - PPM_PULSE_US;
        used += v;
    }

    uint32_t sync = (used + PPM_PULSE_US < PPM_FRAME_US)
                        ? PPM_FRAME_US - used - PPM_PULSE_US
                        : 4000;  // never below the minimum detectable gap
    sym[PPM_NUM_CH].level0 = 1;
    sym[PPM_NUM_CH].duration0 = PPM_PULSE_US;
    sym[PPM_NUM_CH].level1 = 0;
    sym[PPM_NUM_CH].duration1 = sync;

    rmt_transmit_config_t tc = {
        .loop_count = 0,
        .flags.eot_level = 0,  // idle low between frames
    };
    ESP_ERROR_CHECK(rmt_transmit(s_chan, s_encoder, sym, sizeof(sym), &tc));
    rmt_tx_wait_all_done(s_chan, -1);  // returns as the frame completes
}
