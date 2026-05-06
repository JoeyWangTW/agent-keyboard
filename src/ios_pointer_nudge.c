/*
 * agent-keyboard: wake iOS AssistiveTouch pointer routing on BLE reconnect.
 *
 * After encryption comes up on a BLE HID connection, schedule a tiny
 * net-zero mouse-movement report. iOS otherwise requires the user to
 * toggle AssistiveTouch off and on for pointer/scroll to re-engage on
 * each reconnect of a composite keyboard+pointer device. Sending an
 * active-but-cancelling pointer event appears to wake AT's
 * pointer-device evaluation without the manual toggle.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

#include <zmk/hid.h>
#include <zmk/hog.h>

LOG_MODULE_REGISTER(agent_ios_nudge, CONFIG_ZMK_LOG_LEVEL);

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
