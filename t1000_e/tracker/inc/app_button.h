
#ifndef __APP_BUTTON_H
#define __APP_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_PERSS_LONG       3000
#define BUTTON_PRESS_CLICK      500

#define BUTTON_PRESS_ONECE      1
#define BUTTON_PRESS_TWICE      2
#define BUTTON_PRESS_THRICE     3
#define BUTTON_PRESS_FOUR_TIMES 4
#define BUTTON_PRESS_FIVE_TIMES 5

#define TRACKER_STATE_BIT8_USER     0x80

/* ── v22: SOS removed — double-press now toggles turbo scan mode ── */

/*!
 * @brief Init button module
 */
void app_user_button_init( void );

/*!
 * @brief Button detect handler
 */
void app_user_button_det( void );

/*!
 * @brief Power off device
 */
void app_user_power_off( void );

/*!
 * @brief Toggle Power off
 */
void app_toggle_power_off(void);

#ifdef __cplusplus
}
#endif

#endif
