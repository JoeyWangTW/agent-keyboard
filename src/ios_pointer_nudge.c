/*
 * agent-keyboard: wake iOS AssistiveTouch pointer routing on BLE reconnect.
 *
 * On security_changed (link encrypted), fire several pointer reports
 * spread over ~3s. The first net-zero attempt didn't wake AT, so this
 * version sends visible-but-self-cancelling movement + a tiny scroll
 * jiggle. Visible reports double as a diagnostic: if the cursor moves
 * on reconnect, the reports are landing and the issue is purely in
 * iOS's AT engagement logic. If nothing visible happens, reports
 * aren't reaching iOS and the problem is upstream of AT.
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

/* Five passes; each pass moves the cursor right then left, net zero,
 * with a tiny scroll wiggle that visibly cancels itself. The cursor
 * should bounce briefly on reconnect — that's the signal AT can see. */
static void nudge_pass_1(struct k_work *w) { send_report( 20,  0,  0); }
static void nudge_pass_2(struct k_work *w) { send_report(-20,  0,  1); }
static void nudge_pass_3(struct k_work *w) { send_report(  5,  5, -1); }
static void nudge_pass_4(struct k_work *w) { send_report( -5, -5,  0); }
static void nudge_pass_5(struct k_work *w) { send_report(  0,  0,  0); }

static K_WORK_DELAYABLE_DEFINE(work_1, nudge_pass_1);
static K_WORK_DELAYABLE_DEFINE(work_2, nudge_pass_2);
static K_WORK_DELAYABLE_DEFINE(work_3, nudge_pass_3);
static K_WORK_DELAYABLE_DEFINE(work_4, nudge_pass_4);
static K_WORK_DELAYABLE_DEFINE(work_5, nudge_pass_5);

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    ARG_UNUSED(conn);
    if (err || level < BT_SECURITY_L2) {
        return;
    }
    /* Spread across iOS's likely AT-engagement window. */
    k_work_reschedule(&work_1, K_MSEC(300));
    k_work_reschedule(&work_2, K_MSEC(600));
    k_work_reschedule(&work_3, K_MSEC(1200));
    k_work_reschedule(&work_4, K_MSEC(1500));
    k_work_reschedule(&work_5, K_MSEC(2500));
}

BT_CONN_CB_DEFINE(agent_ios_nudge_cb) = {
    .security_changed = security_changed,
};
