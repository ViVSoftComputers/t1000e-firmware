
#ifndef _APP_LED_H_
#define _APP_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

enum APP_LED_STATE_E
{
    APP_LED_BLE_CFG         = 0,
    APP_LED_LORA_JOINING    = 1,
    APP_LED_LORA_JOINED,
    APP_LED_SOS,
    APP_LED_SOS_CONFIRM,
    APP_LED_LORA_DOENLINK,
    APP_LED_DRAIN_ACTIVE,
    APP_LED_IDLE,
};

extern uint8_t app_led_state;

/*!
 * @brief Init led module
 */
void app_led_init( void );

/*!
 * @brief Led of breathe start
 */
void app_led_breathe_start( void );

/*!
 * @brief Led of breathe stop
 */
void app_led_breathe_stop( void );

/*!
 * @brief Led of ble config
 */
void app_led_ble_cfg( void );

/*!
 * @brief Led of lora joined
 */
void app_led_lora_joined( void );

/*!
 * @brief Led of sos
 */
void app_led_sos_run( void );

/*!
 * @brief Led of sos confirm
 */
void app_led_sos_confirm( void );

/*!
 * @brief Led of lora downlink
 */
void app_led_lora_downlink( void );

/*!
 * @brief Led of idle
 */
void app_led_idle( void );

/*!
 * @brief Toggle battery charge dectect
 */
void app_led_bat_new_detect( uint32_t time );

/*!
 * @brief Solid red LED while turbo mode is active, off otherwise.
 *
 * Deliberately independent of app_led_state / the green-LED blink state
 * machine — it's a persistent status, not a transient animation, and
 * needs to be visible regardless of whatever green pattern is playing
 * (e.g. a cache drain running concurrently). Suppresses the battery
 * charge-status indicator on the same LED while active — see
 * app_user_bat_event_timeout_handler().
 */
void app_led_turbo_indicator( bool on );

/*!
 * @brief Fast green blink for as long as a cache drain chain is running.
 * Call app_led_drain_start() once when the chain begins and
 * app_led_drain_stop() once when it ends (success, timeout, or
 * out-of-range abort) — not per individual entry, to avoid flicker.
 */
void app_led_drain_start( void );
void app_led_drain_stop( void );

/*!
 * @brief Flash one LED color N times, blocking. For pairing with a
 * blocking beep pattern so light and sound land on the same instants —
 * not for persistent/ongoing indication (use the *_indicator /
 * *_start/_stop functions above for that).
 *
 * @param [in] red     true = red LED, false = green LED
 * @param [in] count   number of flashes
 * @param [in] on_ms   on-time per flash, in ms
 * @param [in] off_ms  gap between flashes, in ms (not applied after the last)
 */
void app_led_flash( bool red, uint8_t count, uint16_t on_ms, uint16_t off_ms );

#ifdef __cplusplus
}
#endif

#endif
