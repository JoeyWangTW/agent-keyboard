/*
 * agent-keyboard: wake iOS AssistiveTouch pointer routing on BLE reconnect.
 *
 * After encryption comes up on a BLE HID connection, schedule a tiny
 * net-zero mouse-movement report. iOS otherwise requires the user to
 * toggle AssistiveTouch off and on for pointer/scroll to re-engage on
 * each reconnect of a composite keyboard+pointer device. Sending an
 * active-but-cancelling pointer event appears to wake AT's
 * pointer-device evaluation without the manual toggle.
 *
 * ZMK doesn't export hid.h / hog.h to extra modules, so the report
 * struct and the send function are forward-declared here. They have
 * been stable for a while; if ZMK changes the layout, the build will
 * either fail at link time or produce a wrong-sized report.
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

static struct k_work_delayable nudge_work;

static void send_nudge(struct k_work *work) {
    ARG_UNUSED(work);

    struct zmk_hid_mouse_report_body wake = {.d_x = 1};
    int ret = zmk_hog_send_mouse_report(&wake);
    if (ret < 0) {
        LOG_DBG("ios nudge wake report failed: %d", ret);
        return;
    }

    struct zmk_hid_mouse_report_body cancel = {.d_x = -1};
    ret = zmk_hog_send_mouse_report(&cancel);
    if (ret < 0) {
        LOG_DBG("ios nudge cancel report failed: %d", ret);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    ARG_UNUSED(conn);

    if (err) {
        return;
    }
    if (level >= BT_SECURITY_L2) {
        k_work_reschedule(&nudge_work,
                          K_MSEC(CONFIG_ZMK_AGENT_IOS_POINTER_NUDGE_DELAY_MS));
    }
}

BT_CONN_CB_DEFINE(agent_ios_nudge_cb) = {
    .security_changed = security_changed,
};

static int agent_ios_nudge_init(void) {
    k_work_init_delayable(&nudge_work, send_nudge);
    return 0;
}

SYS_INIT(agent_ios_nudge_init, APPLICATION, 90);
