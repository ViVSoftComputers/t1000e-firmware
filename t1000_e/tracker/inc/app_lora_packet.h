

#ifndef __APP_LORA_PACKET_H
#define __APP_LORA_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LORAWAN_APP_DATA_MAX_SIZE 242

#define DATA_ID_UP_PACKET_POWER             0x1e
#define DATA_ID_UP_PACKET_GPS_SEN_ACC_BAT   0x1f
#define DATA_ID_UP_PACKET_WIFI_SEN_ACC_BAT  0x20
#define DATA_ID_UP_PACKET_BLE_SEN_ACC_BAT   0x21
#define DATA_ID_UP_PACKET_GPS_SEN_BAT       0x22
#define DATA_ID_UP_PACKET_WIFI_SEN_BAT      0x23
#define DATA_ID_UP_PACKET_BLE_SEN_BAT       0x24
#define DATA_ID_UP_PACKET_SEN_ACC_BAT       0x25
#define DATA_ID_UP_PACKET_SEN_BAT           0x26

#define DATA_ID_DW_PACKET_INTEVAL_PARAM     0x81
#define DATA_ID_DW_PACKET_BUZER             0x82
#define DATA_ID_DW_PACKET_DRAIN_INTEVAL_PARAM 0x83  /* v31: consumer drain interval, mirrors INTEVAL_PARAM's format */
#define DATA_ID_DW_PACKET_TRACK_TYPE        0x86
#define DATA_ID_DW_PACKET_POWEWR_SEND       0x88
#define DATA_ID_DW_PACKET_REBOOT            0x89
#define DATA_ID_DW_PACKET_SOS_CONTINUOUS    0x8D

/* Reset reason codes reported in the power-on uplink (from NRF_POWER->RESETREAS) */
#define RESET_REASON_NONE          0  /* power-on reset / no prior reset recorded */
#define RESET_REASON_PIN           1  /* reset pin / external reset */
#define RESET_REASON_WATCHDOG      2  /* watchdog timeout (hang) */
#define RESET_REASON_LOCKUP        3  /* CPU lockup (hard fault) */
#define RESET_REASON_SOFT          4  /* software reset (NVIC_SystemReset / APP_ERROR / downlink reboot) */
#define RESET_REASON_OFF           5  /* wake from System OFF */

/*!
 * @brief Uplink power on message
 */
void app_lora_packet_power_on_uplink( void );

/*!
 * @brief Capture the reset reason from the previous boot (RESETREAS) and clear
 * the register. Call once, early in main(), before it can be clobbered.
 */
void app_lora_packet_capture_reset_reason( void );

/*!
 * @brief Decode downlink data
 * 
 * @param [in] Pointer to buffer to be decoded
 * @param [in] Buffer length to be decoded
 */
void app_lora_packet_downlink_decode( uint8_t *buf, uint8_t len );

/*!
 * @brief Load config params
 */
void app_lora_packet_params_load( void );

#ifdef __cplusplus
}
#endif

#endif
