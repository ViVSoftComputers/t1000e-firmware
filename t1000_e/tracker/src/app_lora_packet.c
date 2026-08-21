/*
 * Based on Seeed Studio T1000-E LoRaWAN tracker firmware (MIT License).
 * Cache integration and modifications: Copyright (C) 2026 ViVSoft Computers LLC.
 */


#include "smtc_hal.h"
#include "nrf.h"
#include "sensor.h"
#include "app_board.h"
#include "app_config_param.h"
#include "app_lora_packet.h"
#include "app_tracker_cache.h"

uint8_t app_lora_packet_buffer[LORAWAN_APP_DATA_MAX_SIZE] = { 0 };
uint8_t app_lora_packet_len = 0;

extern uint8_t tracker_scan_type;

extern uint32_t gnss_scan_duration;
extern uint32_t wifi_scan_duration;
extern uint32_t ble_scan_duration;
extern uint32_t tracker_periodic_interval;
extern uint32_t tracker_drain_interval;

extern uint8_t wifi_scan_max;
extern uint8_t ble_scan_max;

extern uint8_t tracker_acc_en;

extern bool adr_user_enable;
extern uint8_t adr_user_dr_min;
extern uint8_t adr_user_dr_max;

/* v22: button_sos_type removed */

extern uint8_t ble_uuid_filter_array[16];
extern uint8_t ble_uuid_filter_num;

extern uint8_t packet_policy;

extern uint8_t tracker_test_mode;

/* Reset reason captured at boot (see app_lora_packet_capture_reset_reason). */
static uint8_t s_reset_reason = RESET_REASON_NONE;

void app_lora_packet_capture_reset_reason( void )
{
    uint32_t rr = NRF_POWER->RESETREAS;
    if( rr & POWER_RESETREAS_DOG_Msk )           s_reset_reason = RESET_REASON_WATCHDOG;
    else if( rr & POWER_RESETREAS_LOCKUP_Msk )   s_reset_reason = RESET_REASON_LOCKUP;
    else if( rr & POWER_RESETREAS_SREQ_Msk )     s_reset_reason = RESET_REASON_SOFT;
    else if( rr & POWER_RESETREAS_RESETPIN_Msk ) s_reset_reason = RESET_REASON_PIN;
    else if( rr & POWER_RESETREAS_OFF_Msk )      s_reset_reason = RESET_REASON_OFF;
    else                                              s_reset_reason = RESET_REASON_NONE;
    NRF_POWER->RESETREAS = rr;  /* write-1-to-clear so the next reset is clean */
}

void app_lora_packet_power_on_uplink( void )
{
    int8_t battery = sensor_bat_sample( );
    BoardVersion_t version = smtc_board_version_get( );

    app_lora_packet_buffer[0] = DATA_ID_UP_PACKET_POWER;
    app_lora_packet_buffer[1] = battery;
    app_lora_packet_buffer[2] = version.Fields.SwMajor;
    app_lora_packet_buffer[3] = version.Fields.SwMinor;
    app_lora_packet_buffer[4] = version.Fields.HwMajor;
    app_lora_packet_buffer[5] = version.Fields.HwMinor;
    app_lora_packet_buffer[6] = tracker_scan_type;
    uint32_t temp = tracker_periodic_interval / 60;
    memcpyr( app_lora_packet_buffer + 7, ( uint8_t *)( &temp ), 2 );
    app_lora_packet_buffer[9] = tracker_acc_en;
    app_lora_packet_buffer[10] = 0;  /* v22: SOS removed */
    app_lora_packet_buffer[11] = wifi_scan_max;
    app_lora_packet_buffer[12] = ble_scan_max;

    app_lora_packet_len = 13;

    app_lora_packet_buffer[app_lora_packet_len++] = FIRMWARE_VERSION;
    app_lora_packet_buffer[app_lora_packet_len++] = s_reset_reason;

    app_lora_packet_buffer[app_lora_packet_len++] = 0xC0;
    app_lora_packet_buffer[app_lora_packet_len++] = 0xDE;

    app_send_frame( app_lora_packet_buffer, app_lora_packet_len, false, true );
}

void app_lora_packet_downlink_decode( uint8_t *buf, uint8_t len )
{
    uint8_t data_id = 0;
    uint32_t tmep_32 = 0;
    bool general_param_update = false;

    if( buf && len )
    {
        data_id = buf[0];
        switch( data_id )
        {
            /* v31: was referenced below (general_param_update's re-run trigger
             * already checked for this data_id) but had no case here, so this
             * downlink never actually did anything -- every byte just fell
             * through to default. Wire format is the one already documented
             * in the README's downlink table: `81 00 00 HH LL`, 5 bytes --
             * data_id, 2 reserved/padding bytes, then minutes as a big-endian
             * uint16 in the last 2 bytes. */
            case DATA_ID_DW_PACKET_INTEVAL_PARAM:
            {
                if( len >= 5 )
                {
                    uint16_t minutes = 0;
                    memcpyr( ( uint8_t * )( &minutes ), buf + 3, 2 );
                    if( minutes >= 1 && minutes <= 1440 )
                    {
                        general_param_update = true;
                        app_param.hardware_info.pos_interval = minutes;
                        tracker_periodic_interval = ( uint32_t )minutes * 60;
                        PRINTF( "tracker_periodic_interval = %u min\r\n", minutes );
                    }
                }
            }
            break;

            /* v31: consumer drain interval, same `83 00 00 HH LL` wire
             * format as INTEVAL_PARAM above. Unlike the scan interval,
             * this doesn't force an immediate drain -- the power-on
             * uplink below already confirms the config changed, and the
             * new value applies on the consumer's own next scheduled
             * tick (schedule_consumer() reads tracker_drain_interval
             * live every time it's called). */
            case DATA_ID_DW_PACKET_DRAIN_INTEVAL_PARAM:
            {
                if( len >= 5 )
                {
                    uint16_t minutes = 0;
                    memcpyr( ( uint8_t * )( &minutes ), buf + 3, 2 );
                    if( minutes >= 1 && minutes <= 1440 )
                    {
                        general_param_update = true;
                        app_param.hardware_info.drain_interval = minutes;
                        tracker_drain_interval = ( uint32_t )minutes * 60;
                        PRINTF( "tracker_drain_interval = %u min\r\n", minutes );
                    }
                }
            }
            break;

            case DATA_ID_DW_PACKET_TRACK_TYPE:
            {
                if(( buf[1] >= 0 && buf[1] <= 1 ) || ( buf[1] >= 3 && buf[1] <= 7 ))
                {
                    general_param_update = true;
                    tracker_scan_type = buf[1];
                    app_param.hardware_info.pos_strategy = buf[1];
                    PRINTF( "tracker_scan_type = %u\r\n", tracker_scan_type );
                }
            }
            break;

            case DATA_ID_DW_PACKET_POWEWR_SEND:
            {
                app_lora_packet_power_on_uplink( );
                PRINTF( "new power on uplink\r\n" );
            }
            break;

            case DATA_ID_DW_PACKET_REBOOT:
            {
                hal_mcu_reset( );
            }
            break;

            case DATA_ID_DW_PACKET_SOS_CONTINUOUS:
            {
                /* v22: SOS removed - downlink SOS toggle is a no-op */
                PRINTF( "lora sos (disabled in v22)\r\n" );
            }
            break;


            case DATA_ID_DW_PACKET_BUZER:
            {
                if( buf[1] == 1 )
                {
                    app_beep_lora_downlink( );
                    app_led_lora_downlink( );
                }
                else if( buf[1] == 0 )
                {
                    app_beep_idle( );
                    app_led_idle( );
                }
            }
            break;

            default:
            break;
        }

        if( general_param_update )
        {
            app_lora_packet_power_on_uplink( );

            if( write_current_param_config( ))
            {
                PRINTF( "LoRa downlink param save ok\r\n" );
            }
            else
            {
                PRINTF( "LoRa downlink param save fail\r\n" );
            }

            if( data_id == DATA_ID_DW_PACKET_TRACK_TYPE || data_id == DATA_ID_DW_PACKET_INTEVAL_PARAM )
            {
                // run a new track
                app_tracker_new_run( 0x00 );
            }
        }
    }
}

void app_lora_packet_params_load( void )
{
    tracker_scan_type = app_param.hardware_info.pos_strategy;

    gnss_scan_duration = ( uint32_t )app_param.hardware_info.gnss_overtime;
    ble_scan_duration = ( uint32_t )app_param.hardware_info.beac_overtime;
    if( ble_scan_duration > 30 ) ble_scan_duration = 30;

    tracker_periodic_interval = (( uint32_t )app_param.hardware_info.pos_interval ) * 60;
    tracker_drain_interval = (( uint32_t )app_param.hardware_info.drain_interval ) * 60;

    wifi_scan_max = app_param.hardware_info.wifi_max;
    ble_scan_max = app_param.hardware_info.beac_max;

    tracker_acc_en = app_param.hardware_info.acc_en;

    adr_user_enable = app_param.lora_info.lr_ADR_en;
    adr_user_dr_min = app_param.lora_info.lr_DR_min;
    adr_user_dr_max = app_param.lora_info.lr_DR_max;

    /* v22: button_sos_type removed */

    memcpy( ble_uuid_filter_array, app_param.hardware_info.beac_uuid, sizeof( ble_uuid_filter_array ));
    ble_uuid_filter_num = app_param.hardware_info.uuid_num;

    packet_policy = app_param.lora_info.Retry;

    tracker_test_mode = app_param.hardware_info.test_mode;
}

