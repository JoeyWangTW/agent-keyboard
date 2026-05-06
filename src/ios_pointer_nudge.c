/*
 * agent-keyboard: wake iOS AssistiveTouch pointer/scroll routing on
 * BLE reconnect, without forcing the user to toggle AT off and on.
 *
 * On security_changed (link encrypted), schedule a small burst of
 * pointer activity:
 *   1. Push the AT cursor away from any idle-parked corner toward
 *      the middle of the screen, where it will be over scrollable
 *      content in most apps. Movement is relative + iOS clamps at
 *      screen edges, so a (+200, +400) push lands in mid-screen on
 *      typical iPhone sizes regardless of starting position.
 *   2. Fire a couple of self-cancelling scroll ticks so iOS's
 *      pointer-routing path sees scroll activity from us.
 *
 * After this, encoder scroll works without the user having to toggle
 * AssistiveTouch off and on on each reconnect.
 *
 * The cursor visibly drifts toward mid-screen on every iPhone
 * reconnect — that's the cost of keeping AT routing engaged.
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

#define DEF_PULSE(N, DX, DY, SY)                                               \
    static void nudge_##N(struct k_work *w) { send_report(DX, DY, SY); }       \
    static K_WORK_DELAYABLE_DEFINE(work_##N, nudge_##N)

/* Drive cursor into mid-screen, then wake scroll. */
DEF_PULSE(a,  200,  400,  0);  /* push to middle */
DEF_PULSE(b,    0,    0,  1);  /* scroll wake down */
DEF_PULSE(c,    0,    0, -1);  /* scroll wake up */
DEF_PULSE(d,    5,    5,  0);  /* small twitch keeps cursor visible */
DEF_PULSE(e,    0,    0,  0);  /* idle */

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    ARG_UNUSED(conn);
    if (err || level < BT_SECURITY_L2) {
        return;
    }
    k_work_reschedule(&work_a, K_MSEC(400));
    k_work_reschedule(&work_b, K_MSEC(700));
    k_work_reschedule(&work_c, K_MSEC(900));
    k_work_reschedule(&work_d, K_MSEC(1200));
    k_work_reschedule(&work_e, K_MSEC(1800));
}

BT_CONN_CB_DEFINE(agent_ios_nudge_cb) = {
    .security_changed = security_changed,
};
