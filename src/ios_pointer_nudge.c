/*
 * agent-keyboard: wake iOS AssistiveTouch pointer/scroll routing on
 * BLE reconnect, without forcing the user to toggle AT off and on.
 *
 * On security_changed (link encrypted), schedule a small burst of
 * pointer activity over ~1.5s. Empirically a sustained burst of
 * cursor movement plus self-cancelling scroll ticks is enough to
 * wake iOS pointer routing on reconnect, after which scroll from
 * the encoder works in the apps that respect HID scroll (Safari,
 * Mail, Notes, X, scrollable Settings panes, etc.).
 *
 * The values here are a balance: small enough that the cursor
 * bounce on reconnect is unobtrusive, large enough to reliably
 * trip iOS's pointer-recognition path.
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

/* Net-zero cursor displacement and net-zero scroll. */
DEF_PULSE(a,   80,    0,  1);
DEF_PULSE(b,  -80,    0,  0);
DEF_PULSE(c,    0,   40, -1);
DEF_PULSE(d,    0,  -40,  0);

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    ARG_UNUSED(conn);
    if (err || level < BT_SECURITY_L2) {
        return;
    }
    k_work_reschedule(&work_a, K_MSEC(500));
    k_work_reschedule(&work_b, K_MSEC(700));
    k_work_reschedule(&work_c, K_MSEC(1100));
    k_work_reschedule(&work_d, K_MSEC(1300));
}

BT_CONN_CB_DEFINE(agent_ios_nudge_cb) = {
    .security_changed = security_changed,
};
