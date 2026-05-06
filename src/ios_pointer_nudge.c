/*
 * agent-keyboard: iOS AssistiveTouch pointer-routing diagnostic.
 *
 * Final-answer diagnostic build: send unmistakably large pointer
 * activity at sustained intervals after each BLE security_changed.
 * If you see the cursor jump 200px right then back, or the page
 * scroll several ticks and unscroll on reconnect, our reports are
 * landing in iOS — meaning AT is gating scroll on a UI-level toggle
 * that no firmware activity can replicate. If you see nothing
 * visible at all, the reports aren't being delivered to iOS and
 * the problem is on the BLE/CCC subscription path.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <stdint.h>

LOG_MODULE_REGISTER(agent_ios_nudge, CONFIG_ZMK_LOG_LEVEL);

struct zmk_hid_mouse_report_body {
    uint8_t buttons;
    int16_t d_x;
    int16_t d_y;
    int16_t d_scroll_y;
    int16_t d_scroll_x;
} __packed;

extern int zmk_hog_send_mouse_report(struct zmk_hid_mouse_report_body *body);

static void send_report(int16_t dx, int16_t dy, int16_t scroll_y) {
    struct zmk_hid_mouse_report_body r = {
        .d_x = dx, .d_y = dy, .d_scroll_y = scroll_y,
    };
    int ret = zmk_hog_send_mouse_report(&r);
    if (ret < 0) {
        LOG_DBG("nudge report failed: %d", ret);
    }
}

/* Loud, self-cancelling pulses. Unmistakably visible if delivered. */
#define DEF_PULSE(N, DX, DY, SY)                                               \
    static void nudge_##N(struct k_work *w) { send_report(DX, DY, SY); }       \
    static K_WORK_DELAYABLE_DEFINE(work_##N, nudge_##N)

DEF_PULSE(a,  200,    0,  0);
DEF_PULSE(b, -200,    0,  3);
DEF_PULSE(c,    0,  100, -3);
DEF_PULSE(d,    0, -100,  0);
DEF_PULSE(e,  150,    0,  3);
DEF_PULSE(f, -150,    0, -3);
DEF_PULSE(g,    0,    0,  0);

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    ARG_UNUSED(conn);
    if (err || level < BT_SECURITY_L2) {
        return;
    }
    /* Sustained burst across iOS's likely subscription window. */
    k_work_reschedule(&work_a, K_MSEC(500));
    k_work_reschedule(&work_b, K_MSEC(800));
    k_work_reschedule(&work_c, K_MSEC(1500));
    k_work_reschedule(&work_d, K_MSEC(1800));
    k_work_reschedule(&work_e, K_MSEC(3000));
    k_work_reschedule(&work_f, K_MSEC(3300));
    k_work_reschedule(&work_g, K_MSEC(5000));
}

BT_CONN_CB_DEFINE(agent_ios_nudge_cb) = {
    .security_changed = security_changed,
};
