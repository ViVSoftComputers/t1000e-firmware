/*
 * Based on Seeed Studio T1000-E LoRaWAN tracker firmware (MIT License).
 * Cache integration and modifications: Copyright (C) 2026 ViVSoft Computers LLC.
 */

#include "main_lorawan_tracker.h"
#include "lorawan_key_config.h"
#include "smtc_board.h"
#include "smtc_hal.h"
#include "apps_modem_common.h"
#include "apps_modem_event.h"
#include "smtc_modem_api.h"
#include "device_management_defs.h"
#include "smtc_board_ralf.h"
#include "apps_utilities.h"
#include "smtc_modem_utilities.h"
#include "lr11xx_system.h"

#include "app_timer.h"

#include "app_board.h"
#include "app_ble_all.h"
#include "app_config_param.h"
#include "app_at_fds_datas.h"
#include "app_at_command.h"
#include "app_lora_packet.h"
#include "app_user_timer.h"
#include "app_button.h"
#include "app_led.h"
#include "app_beep.h"
#include "app_tracker_cache.h"

/* Forward declaration — declared in smtc_modem.c but not in smtc_modem_api.h */
extern smtc_modem_return_code_t smtc_modem_set_region_duty_cycle( uint8_t stack_id, bool status );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*!
 * @brief Stack identifier
 */
static uint8_t stack_id = 0;

/*!
 * @brief Modem radio
 */
static ralf_t* modem_radio;

/*!
 * @brief User application data
 */
static uint8_t app_data_buffer[LORAWAN_APP_DATA_MAX_SIZE];
static uint8_t app_data_len = 0;

static uint8_t adr_custom_list_eu868_default[16] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5 }; // SF12,SF12,SF12,SF11,SF11,SF11,SF10,SF10,SF10,SF9,SF9,SF9,SF8,SF8,SF7,SF7
static uint8_t adr_custom_list_us915_default[16] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 }; // SF9,SF9,SF9,SF9,SF9,SF8,SF8,SF8,SF8,SF8,SF7,SF7,SF7,SF7,SF7
static uint8_t adr_custom_list_au915_default[16] = { 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 }; // SF9,SF9,SF9,SF9,SF9,SF8,SF8,SF8,SF8,SF8,SF7,SF7,SF7,SF7,SF7
static uint8_t adr_custom_list_as923_default[16] = { 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 }; // SF9,SF9,SF9,SF9,SF9,SF8,SF8,SF8,SF8,SF8,SF7,SF7,SF7,SF7,SF7
static uint8_t adr_custom_list_kr920_default[16] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5 }; // SF12,SF12,SF12,SF11,SF11,SF11,SF10,SF10,SF10,SF9,SF9,SF9,SF8,SF8,SF7,SF7
static uint8_t adr_custom_list_in865_default[16] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5 }; // SF12,SF12,SF12,SF11,SF11,SF11,SF10,SF10,SF10,SF9,SF9,SF9,SF8,SF8,SF7,SF7
static uint8_t adr_custom_list_ru864_default[16] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5 }; // SF12,SF12,SF12,SF11,SF11,SF11,SF10,SF10,SF10,SF9,SF9,SF9,SF8,SF8,SF7,SF7

static uint8_t tracker_scan_status = 0;
static uint32_t tracker_scan_begin = 0;
static uint8_t scan_stall_count = 0;       /* watchdog: resets stuck scan state */

uint8_t tracker_scan_type = 0;

uint32_t gnss_scan_duration = 15;            // in second
uint32_t wifi_scan_duration = 3;            // in second
uint32_t ble_scan_duration = 3;             // in second
uint32_t tracker_periodic_interval = 300;   // 5 min default (changed via downlink)
uint32_t tracker_drain_interval = 1500;  /* consumer drains every 25 min, independent of scan */

/* Turbo mode — double-press toggles 30-second scan interval */
static bool     turbo_active = false;
static uint32_t saved_periodic_interval = 60;
static uint32_t gps_scan_start_time = 0;    /* timeout watchdog */

uint8_t wifi_scan_max = 3;
uint8_t ble_scan_max = 3;

uint8_t tracker_acc_en = 0;

bool adr_user_enable = true;
uint8_t adr_user_dr_min = 0;
uint8_t adr_user_dr_max = 0;
uint8_t adr_custom_list_region[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

uint8_t packet_policy = RETRY_STATE_1N;

bool duty_cycle_enable = true;

uint8_t tracker_test_mode = 0;

uint8_t tracker_gps_scan_len = 0;
uint8_t tracker_gps_scan_data[64] = { 0 };

uint8_t tracker_wifi_scan_len = 0;
uint8_t tracker_wifi_scan_data[64] = { 0 };

uint8_t tracker_ble_scan_len = 0;
uint8_t tracker_ble_scan_data[64] = { 0 };

uint8_t tracker_scan_temp_len = 0;
uint8_t tracker_scan_data_temp[64] = { 0 };

bool scan_result = false;
static bool cache_drain_active = false;
static uint32_t drain_start_time_s = 0;    /* per-entry TX watchdog */
static uint32_t drain_overall_start_s = 0; /* overall drain chain timeout */
static bool force_drain_pending = false;    /* completion beep after force-drain */
int8_t scan_result_num = 0;

/* Independent timers for producer and consumer.
 * Each side schedules its own next tick. on_modem_alarm() picks the sooner. */
static uint32_t producer_next_s = 0;  /* absolute RTC seconds */
static uint32_t consumer_next_s = 0;  /* absolute RTC seconds */

uint8_t event_state = 0;
  /* queued presses during scan */
static bool producer_pending = false;   /* ISR-safe kick flag */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*!
 * @brief   Send an application frame on LoRaWAN port defined by LORAWAN_APP_PORT
 *
 * @param [in] buffer     Buffer containing the LoRaWAN buffer
 * @param [in] length     Payload length
 * @param [in] confirmed  Send a confirmed or unconfirmed uplink [false : unconfirmed / true : confirmed]
 */
static void send_frame( const uint8_t* buffer, const uint8_t length, const bool confirmed );

/*!
 * @brief Parse the received downlink
 *
 * @remark Demonstrates how a TLV-encoded command sequence received by downlink can control the state of an LED. It can
 * easily be extended to handle other commands received on the same port or another port.
 *
 * @param [in] port LoRaWAN port
 * @param [in] payload Payload Buffer
 * @param [in] size Payload size
 */
static void parse_downlink_frame( uint8_t port, const uint8_t* payload, uint8_t size );

/*!
 * @brief Reset event callback
 *
 * @param [in] reset_count reset counter from the modem
 */
static void on_modem_reset( uint16_t reset_count );

/*!
 * @brief Network Joined event callback
 */
static void on_modem_network_joined( void );

/*!
 * @brief Alarm event callback
 */
static void on_modem_alarm( void );

/*!
 * @brief Tx done event callback
 *
 * @param [in] status tx done status @ref smtc_modem_event_txdone_status_t
 */
static void on_modem_tx_done( smtc_modem_event_txdone_status_t status );

/* ---- Independent timer wrappers ---- */

static void sync_alarm( void );
static void schedule_producer( uint32_t delay_s )
{
    producer_next_s = hal_rtc_get_time_s( ) + delay_s;
    sync_alarm( );
}

static void schedule_consumer( uint32_t delay_s )
{
    consumer_next_s = hal_rtc_get_time_s( ) + delay_s;
    sync_alarm( );
}

static void sync_alarm( void )
{
    uint32_t now = hal_rtc_get_time_s( );
    uint32_t pd = ( producer_next_s > now ) ? ( producer_next_s - now ) : 0;
    uint32_t cd = ( consumer_next_s > now ) ? ( consumer_next_s - now ) : 0;

    uint32_t next;
    if( pd == 0 && cd == 0 )         next = 1;
    else if( pd == 0 )               next = cd;
    else if( cd == 0 )               next = pd;
    else                             next = ( pd < cd ) ? pd : cd;

    smtc_modem_alarm_start_timer( next > 0 ? next : 1 );
}


static void cache_consumer_trigger( void );

/*!
 * @brief Downlink data event callback.
 *
 * @param [in] rssi       RSSI in signed value in dBm + 64
 * @param [in] snr        SNR signed value in 0.25 dB steps
 * @param [in] rx_window  RX window
 * @param [in] port       LoRaWAN port
 * @param [in] payload    Received buffer pointer
 * @param [in] size       Received buffer size
 */
static void on_modem_down_data( int8_t rssi, int8_t snr, smtc_modem_event_downdata_window_t rx_window, uint8_t port,
                                const uint8_t* payload, uint8_t size );

/*!
 * @brief 
 */
static void app_tracker_scan_process( void );

/*!
 * @}
 */

/*
 * ----------------------------------------------------------------------------- 
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Main application entry point.
 */
int main( void )
{
    static apps_modem_event_callback_t smtc_event_callback = {
        .adr_mobile_to_static  = NULL,
        .alarm                 = on_modem_alarm,
        .almanac_update        = NULL,
        .down_data             = on_modem_down_data,
        .join_fail             = NULL,
        .joined                = on_modem_network_joined,
        .link_status           = NULL,
        .mute                  = NULL,
        .new_link_adr          = NULL,
        .reset                 = on_modem_reset,
        .set_conf              = NULL,
        .stream_done           = NULL,
        .time_updated_alc_sync = NULL,
        .tx_done               = on_modem_tx_done,
        .upload_done           = NULL,
    };

    /* Initialise the ralf_t object corresponding to the board */
    modem_radio = smtc_board_initialise_and_get_ralf( );

    /* Init board and peripherals */
    hal_mcu_init( );
    fds_init_write( );
    smtc_board_init_periph( );
    app_lora_packet_params_load( );

    ret_code_t err_code;
    err_code = app_timer_init( );
    APP_ERROR_CHECK( err_code );

    hal_mcu_wait_ms( 500 ); // wait for stable

    app_user_timers_init( );
    app_user_button_init( );
    app_ble_all_init( );
    app_led_init( );
    app_beep_init( );

    app_beep_boot_up( );

    if( tracker_scan_type == TRACKER_SCAN_GNSS_ONLY 
        || tracker_scan_type == TRACKER_SCAN_WIFI_GNSS 
        || tracker_scan_type == TRACKER_SCAN_GNSS_WIFI 
        || tracker_scan_type == TRACKER_SCAN_BLE_GNSS 
        || tracker_scan_type == TRACKER_SCAN_BLE_WIFI_GNSS )
    {
        gnss_init( );
        gnss_scan_start( );
        hal_mcu_wait_ms( 3000 );
        gnss_scan_stop( );
    }

    if( tracker_acc_en )
    {
        hal_gpio_init_out( ACC_POWER, HAL_GPIO_SET );
        qma6100p_init( );
    }

APP_MAIN:
    /* Init the Lora Basics Modem event callbacks */
    apps_modem_event_init( &smtc_event_callback );

    /* Init the modem and use smtc_event_process as event callback, please note that the callback will be called
     * immediately after the first call to modem_run_engine because of the reset detection */
    smtc_modem_init( modem_radio, &apps_modem_event_process );

    HAL_DBG_TRACE_MSG( "\n" );
    HAL_DBG_TRACE_INFO( "###### ===== T1000-E HERMES v14-CACHE 20260719 ==== ######\n\n" );

    /* LoRa Basics Modem Version */
    apps_modem_common_display_lbm_version( );

    /* Configure the partial low power mode */
    hal_mcu_partial_sleep_enable( APP_PARTIAL_SLEEP );

    while( 1 )
    {
        /* ISR-safe producer kick: button ISR sets producer_pending,
         * we call schedule_producer() here in main-loop context. */
        if( producer_pending )
        {
            producer_pending = false;
            schedule_producer( 1 );
        }
        /* Execute modem runtime, this function must be called again in sleep_time_ms milliseconds or sooner. */
        uint32_t sleep_time_ms = smtc_modem_run_engine( );
        /* go in low power */
        hal_mcu_set_sleep_for_ms( sleep_time_ms );
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/*!
 * @brief LoRa Basics Modem event callbacks called by smtc_event_process function
 */

static void on_modem_reset( uint16_t reset_count )
{
    HAL_DBG_TRACE_INFO( "Application parameters:\n" );
    HAL_DBG_TRACE_INFO( "  - LoRaWAN uplink Fport = %d\n", LORAWAN_APP_PORT );
    HAL_DBG_TRACE_INFO( "  - Confirmed uplink     = %s\n", ( LORAWAN_CONFIRMED_MSG_ON == true ) ? "Yes" : "No" );

    apps_modem_common_configure_lorawan_params( stack_id );

    uint8_t ativation_mode;
    ativation_mode = smtc_modem_get_activation_mode( stack_id );
    if( ativation_mode == 0 ) // OTAA
    {
        app_led_breathe_start( );
        ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( stack_id ) );
    }
    else // ABP, manual run joined init
    {
        on_modem_network_joined( );
    }
}

static void custom_lora_adr_compute( uint8_t min, uint8_t max, uint8_t *buf )
{
    uint8_t temp = max - min + 1;
    uint8_t num = 16 / temp;
    uint8_t remain = 16 % temp;
    uint8_t offset = 0;

    for( uint8_t i = 0; i < temp; i++ )
    {
        for( uint8_t j = 0; j < num; j++ )
        {
            buf[i * num + j + offset] = min + i;
        }

        if(( 16 % temp != 0 ) && ( i < remain ))
        {
            buf[( i + 1 ) * num + offset] = min + i;
            offset += 1;
        }
    }
}

static void on_modem_network_joined( void )
{
    smtc_modem_region_t region;
    ASSERT_SMTC_MODEM_RC( smtc_modem_get_region( stack_id, &region ));

    if( app_led_state != APP_LED_BLE_CFG )
    {
        uint8_t ativation_mode;
        ativation_mode = smtc_modem_get_activation_mode( stack_id );
        if( ativation_mode == 0 ) // OTAA
        {
            app_beep_joined( );
            app_led_breathe_stop( );
            app_led_lora_joined( );
        }
    }

    if( adr_user_enable == false )
    {
        /* Set the ADR profile based on selected region */
        switch( region )
        {
            case SMTC_MODEM_REGION_EU_868:
                HAL_DBG_TRACE_INFO( "Set ADR profile for EU868\n" );
                if( adr_user_dr_min >= LORAWAN_EU868_DR_MIN && adr_user_dr_max <= LORAWAN_EU868_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_eu868_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_eu868_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_US_915:
                HAL_DBG_TRACE_INFO( "Set ADR profile for US915\n" );
                if( adr_user_dr_min >= LORAWAN_US915_DR_MIN && adr_user_dr_max <= LORAWAN_US915_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_us915_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_us915_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_AU_915:
                HAL_DBG_TRACE_INFO( "Set ADR profile for AU915\n" );
                if( adr_user_dr_min >= LORAWAN_AU915_DR_MIN && adr_user_dr_max <= LORAWAN_AU915_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_au915_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_au915_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_AS_923_GRP1:
            case SMTC_MODEM_REGION_AS_923_GRP2:
            case SMTC_MODEM_REGION_AS_923_GRP3:
            case SMTC_MODEM_REGION_AS_923_GRP4:
            case SMTC_MODEM_REGION_AS_923_HELIUM_1:
            case SMTC_MODEM_REGION_AS_923_HELIUM_2:
            case SMTC_MODEM_REGION_AS_923_HELIUM_3:
            case SMTC_MODEM_REGION_AS_923_HELIUM_4:
            case SMTC_MODEM_REGION_AS_923_HELIUM_1B:
                HAL_DBG_TRACE_INFO( "Set ADR profile for AS923\n" );
                if( adr_user_dr_min >= LORAWAN_AS923_DR_MIN && adr_user_dr_max <= LORAWAN_AS923_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_as923_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_as923_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_KR_920:
                HAL_DBG_TRACE_INFO( "Set ADR profile for KR920\n" );
                if( adr_user_dr_min >= LORAWAN_KR920_DR_MIN && adr_user_dr_max <= LORAWAN_KR920_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_kr920_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_kr920_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_IN_865:
                HAL_DBG_TRACE_INFO( "Set ADR profile for IN865\n" );
                if( adr_user_dr_min >= LORAWAN_IN865_DR_MIN && adr_user_dr_max <= LORAWAN_IN865_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_in865_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_in865_default, 16 );
                }
            break;

            case SMTC_MODEM_REGION_RU_864:
                HAL_DBG_TRACE_INFO( "Set ADR profile for RU864\n" );
                if( adr_user_dr_min >= LORAWAN_RU864_DR_MIN && adr_user_dr_max <= LORAWAN_RU864_DR_MAX )
                {
                    if( adr_user_dr_min == adr_user_dr_max )
                    {
                        memset( adr_custom_list_region, adr_user_dr_min, 16 );
                    }
                    else if( adr_user_dr_min > adr_user_dr_max )
                    {
                        memcpy( adr_custom_list_region, adr_custom_list_ru864_default, 16 );
                    }
                    else
                    {
                        custom_lora_adr_compute( adr_user_dr_min, adr_user_dr_max, adr_custom_list_region );
                    }
                }
                else
                {
                    memcpy( adr_custom_list_region, adr_custom_list_ru864_default, 16 );
                }
            break;

            default:
                HAL_DBG_TRACE_ERROR( "Region not supported in this example, could not set custom ADR profile\n" );
            break;
        }

        HAL_DBG_TRACE_PRINTF( "User ADR list: " ); // just for test
        for( uint8_t i = 0; i < 16; i++ )
        {
            HAL_DBG_TRACE_PRINTF( "%d ", adr_custom_list_region[i] ); // just for test
        }
        HAL_DBG_TRACE_PRINTF( "\r\n" ); // just for test

        ASSERT_SMTC_MODEM_RC( smtc_modem_adr_set_profile( stack_id, SMTC_MODEM_ADR_PROFILE_CUSTOM, adr_custom_list_region ));
        // ASSERT_SMTC_MODEM_RC( smtc_modem_set_nb_trans( stack_id, custom_nb_trans_region ));
    }

    switch( region )
    {
        case SMTC_MODEM_REGION_EU_868:
        case SMTC_MODEM_REGION_RU_864:
            if( duty_cycle_enable == false )
            {
                smtc_modem_set_region_duty_cycle( stack_id, false );
                modem_set_duty_cycle_disabled_by_host( true );
            }
        break;

        default:
        break;
    }

    app_led_bat_new_detect( 3000 );

    app_lora_packet_power_on_uplink( );

    schedule_producer( 15 );
}

static void on_modem_alarm( void )
{
    uint32_t now = hal_rtc_get_time_s( );

    /* Consumer: drain if its timer expired AND cache has entries */
    if( now >= consumer_next_s )
    {
        if( tracker_cache_count( ) > 0 && !cache_drain_active )
        {
            cache_consumer_trigger( );
            /* drain chain will call schedule_consumer() for next consumer tick */
        }
        else if( tracker_cache_count( ) == 0 )
        {
            /* Nothing to drain.
             * If force-drain was requested, beep completion now —
             * no TX-done will ever fire since there's nothing to send. */
            if( force_drain_pending )
            {
                force_drain_pending = false;
                hal_pwm_init( 2000 );
                hal_beep_on( );
                hal_mcu_wait_ms( 500 );
                hal_beep_off( );
                hal_pwm_deinit( );
            }
            schedule_consumer( tracker_drain_interval );
        }
        else
        {
            /* Cache has entries but drain is still in progress.
             * Check again soon — avoids spinning on every alarm tick.
             * Watchdog: if TX hasn't completed in 120s, abort drain
             * and wait for next scheduled interval. */
            uint32_t drain_elapsed = now - drain_overall_start_s;
            if( drain_elapsed > 120 )
            {
                cache_drain_active = false;   /* force timeout */
                drain_overall_start_s = 0;    /* reset for next drain chain */
                schedule_consumer( tracker_drain_interval );
            }
            else
            {
                schedule_consumer( 3 );
            }
        }
    }

    /* Producer: scan if its timer expired */
    if( now >= producer_next_s )
    {
        app_tracker_scan_process( );
        /* scan process calls schedule_producer() for next producer tick */
    }

    /* Always re-arm alarm to the sooner of the two timers */
    sync_alarm( );
}


/* ---- Independent timer wrappers ---- */

static void on_modem_tx_done( smtc_modem_event_txdone_status_t status )
{
    static uint32_t uplink_count = 0;
    HAL_DBG_TRACE_INFO( "Uplink count: %d\n", ++uplink_count );

    if( status == SMTC_MODEM_EVENT_TXDONE_CONFIRMED )
    {
        /* In range — fast drain remaining entries */
        tracker_cache_pop( );
        cache_drain_active = false;

        if( tracker_cache_count( ) > 0 )
        {
            schedule_consumer( 3 );
        }
        else
        {
            /* Cache empty — if force-drain was requested, beep completion. */
            if( force_drain_pending )
            {
                force_drain_pending = false;
                hal_pwm_init( 2000 );
                hal_beep_on( );
                hal_mcu_wait_ms( 500 );
                hal_beep_off( );
                hal_pwm_deinit( );
            }
            drain_overall_start_s = 0;
            schedule_consumer( tracker_drain_interval );
        }
    }
    else
    {
        /* Out of range (SENT/NOT_SENT) — stop, retry next schedule. */
        drain_overall_start_s = 0;
        cache_drain_active = false;
        if( tracker_cache_count( ) == 0 )
        {
            schedule_consumer( tracker_drain_interval );
        }
    }
}

/* Trigger cache consumer: start drain if not active */
static void cache_consumer_trigger( void )
{
    if( !cache_drain_active && tracker_cache_count( ) > 0 )
    {
        uint8_t entry_len = 0;
        uint32_t entry_ts = 0;
        uint8_t *entry = tracker_cache_get( 0, &entry_len, &entry_ts );
        if( entry && entry_len > 0 )
        {
            cache_drain_active = true;
            drain_start_time_s = hal_rtc_get_time_s( );  /* per-entry watchdog */
            if( drain_overall_start_s == 0 )
            {
                drain_overall_start_s = hal_rtc_get_time_s( );  /* drain chain start */
            }
            /* Use CONFIRMED — only pop on real ACK */
            if( !app_send_frame( entry, entry_len, true, false ) )
            {
                cache_drain_active = false;  /* send failed — retry next tick */
            }
        }
    }
}

static void on_modem_down_data( int8_t rssi, int8_t snr, smtc_modem_event_downdata_window_t rx_window, uint8_t port,
                                const uint8_t* payload, uint8_t size )
{
    HAL_DBG_TRACE_INFO( "Downlink received:\n" );
    HAL_DBG_TRACE_INFO( "  - LoRaWAN Fport = %d\n", port );
    HAL_DBG_TRACE_INFO( "  - Payload size  = %d\n", size );
    HAL_DBG_TRACE_INFO( "  - RSSI          = %d dBm\n", rssi - 64 );
    HAL_DBG_TRACE_INFO( "  - SNR           = %d dB\n", snr >> 2 );

    switch( rx_window )
    {
        case SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RX1:
        {
            HAL_DBG_TRACE_INFO( "  - Rx window     = %s\n", xstr( SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RX1 ) );
            break;
        }
        case SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RX2:
        {
            HAL_DBG_TRACE_INFO( "  - Rx window     = %s\n", xstr( SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RX2 ) );
            break;
        }
        case SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RXC:
        {
            HAL_DBG_TRACE_INFO( "  - Rx window     = %s\n", xstr( SMTC_MODEM_EVENT_DOWNDATA_WINDOW_RXC ) );
            break;
        }
    }

    if( size != 0 )
    {
        HAL_DBG_TRACE_ARRAY( "Payload", payload, size );

        if( port == LORAWAN_APP_PORT )
        {
            app_lora_packet_downlink_decode( payload, size );
        }
    }
}

static void app_tracker_ble_scan_begin( void )
{
    tracker_ble_scan_len = 0;
    memset( tracker_ble_scan_data, 0, sizeof( tracker_ble_scan_data ));
    ble_scan_start( );
}

static void app_tracker_ble_scan_end( void )
{
    ble_scan_stop( );
    ble_get_results( tracker_ble_scan_data, &tracker_ble_scan_len );
    ble_display_results( );
    uint8_t len_max = ble_scan_max * 7;
    if( tracker_ble_scan_len > len_max ) tracker_ble_scan_len = len_max;
    if( tracker_ble_scan_len ) scan_result_num ++;
    if( scan_result_num > 3 ) scan_result_num = 3;
    if( tracker_test_mode == 0 && tracker_ble_scan_len ) scan_result = true;    
}

static void app_tracker_wifi_scan_begin( void )
{
    tracker_wifi_scan_len = 0;
    memset( tracker_wifi_scan_data, 0, sizeof( tracker_wifi_scan_data ));
    wifi_scan_start( modem_radio );
}

static void app_tracker_wifi_scan_end( void )
{
    wifi_scan_stop( modem_radio );
    wifi_get_results( modem_radio, tracker_wifi_scan_data, &tracker_wifi_scan_len );
    wifi_display_results( );
    uint8_t len_max = wifi_scan_max * 7;
    if( tracker_wifi_scan_len > len_max ) tracker_wifi_scan_len = len_max;
    if( tracker_wifi_scan_len ) scan_result_num ++;
    if( scan_result_num > 3 ) scan_result_num = 3;
    if( tracker_test_mode == 0 && tracker_wifi_scan_len ) scan_result = true;
}

static void app_tracker_gnss_scan_begin( void )
{
    tracker_gps_scan_len = 0;
    memset( tracker_gps_scan_data, 0, sizeof( tracker_gps_scan_data ));
    gnss_scan_start( );
}

static void app_tracker_gnss_scan_end( void )
{
    static int32_t lat = 0, lon = 0;
    gnss_scan_stop( );
    if( gnss_get_fix_status( ))
    {
        gnss_get_position( &lat, &lon );
        HAL_DBG_TRACE_PRINTF( "lat: %u, lon: %u\n\n", lat, lon );
        memcpyr( tracker_gps_scan_data, ( uint8_t *)&lon, 4 );
        memcpyr( tracker_gps_scan_data + 4, ( uint8_t *)&lat, 4 );
        tracker_gps_scan_len = 8;
    }
    else
    {
        HAL_DBG_TRACE_ERROR( "GNSS fix fail\n\n" );
    }
    if( tracker_gps_scan_len ) scan_result_num ++;
    if( scan_result_num > 3 ) scan_result_num = 3;
    if( tracker_test_mode == 0 && tracker_gps_scan_len ) scan_result = true;
}

static void app_tracker_scan_result_send( void )
{
    bool send_ok = false;
    bool confirm = false;
    int16_t temp = 0;
    uint16_t light = 0;
    int8_t battery = 0;
    int16_t ax = 0, ay = 0, az = 0;
    bool gps_had_fix = ( tracker_gps_scan_len > 0 );  /* v22: capture before reset */

    /* v23: motion gate — skip cache save if within 25m of last known position */
    static int32_t last_lat = 0, last_lon = 0;
    static uint32_t last_tx_time = 0;   /* RTC seconds of last successful save */
    bool moved = true;  /* default: allow save (first run, no GPS, or moved) */

    if(( packet_policy == RETRY_STATE_1C ) || ( event_state == TRACKER_STATE_BIT8_USER ))
    {
        confirm = true;
    }

    memset( tracker_scan_data_temp, 0, sizeof( tracker_scan_data_temp ));
    tracker_scan_temp_len = 0;

    battery = sensor_bat_sample( );
    temp = sensor_ntc_sample( );
    light = sensor_lux_sample( );

    if( tracker_acc_en )
    {
        qma6100p_read_raw_data( &ax, &ay, &az );
    }

    PRINTF( "tracker_gps_scan_len: %d\r\n", tracker_gps_scan_len );
    PRINTF( "tracker_wifi_scan_len: %d\r\n", tracker_wifi_scan_len );
    PRINTF( "tracker_ble_scan_len: %d\r\n", tracker_ble_scan_len );
    PRINTF( "scan_result_num: %d\r\n", scan_result_num );

    if( tracker_gps_scan_len == 0 && tracker_wifi_scan_len == 0 && tracker_ble_scan_len == 0 )
    {
        scan_result_num = 1;

        if( tracker_acc_en )
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_SEN_ACC_BAT;
        }
        else
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_SEN_BAT;
        }
        
        tracker_scan_data_temp[1] = event_state;
        tracker_scan_data_temp[2] = battery;
        memcpyr( tracker_scan_data_temp + 3, ( uint8_t *)( &temp ), 2 );
        memcpyr( tracker_scan_data_temp + 5, ( uint8_t *)( &light ), 2 );
        tracker_scan_temp_len += 7;

        if( tracker_acc_en )
        {
            memcpyr( tracker_scan_data_temp + 7, ( uint8_t *)( &ax ), 2 );
            memcpyr( tracker_scan_data_temp + 9, ( uint8_t *)( &ay ), 2 );
            memcpyr( tracker_scan_data_temp + 11, ( uint8_t *)( &az ), 2 );
            tracker_scan_temp_len += 6;
        }

        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xBE;
        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xEF;

        tracker_cache_save( tracker_scan_data_temp, tracker_scan_temp_len );
        send_ok = true;
    }
    else if( tracker_gps_scan_len )
    {
        if( tracker_acc_en )
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_GPS_SEN_ACC_BAT;
        }
        else
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_GPS_SEN_BAT;
        }

        tracker_scan_data_temp[1] = event_state;
        tracker_scan_data_temp[2] = battery;
        memcpyr( tracker_scan_data_temp + 3, ( uint8_t *)( &temp ), 2 );
        memcpyr( tracker_scan_data_temp + 5, ( uint8_t *)( &light ), 2 );
        tracker_scan_temp_len += 7;

        if( tracker_acc_en )
        {
            memcpyr( tracker_scan_data_temp + 7, ( uint8_t *)( &ax ), 2 );
            memcpyr( tracker_scan_data_temp + 9, ( uint8_t *)( &ay ), 2 );
            memcpyr( tracker_scan_data_temp + 11, ( uint8_t *)( &az ), 2 );
            tracker_scan_temp_len += 6;
        }

        memcpy( tracker_scan_data_temp + tracker_scan_temp_len, tracker_gps_scan_data, tracker_gps_scan_len );
        tracker_scan_temp_len += tracker_gps_scan_len;

        /* v23: embed GPS epoch time (4 bytes, little-endian) after lon+lat */
        uint32_t epoch = gnss_get_epoch( );
        memcpyr( tracker_scan_data_temp + tracker_scan_temp_len, ( uint8_t *)( &epoch ), 4 );
        tracker_scan_temp_len += 4;

        /* v23: motion gate — extract current position, check distance */
        {
            int32_t cur_lat, cur_lon;
            memcpyr( ( uint8_t *)( &cur_lon ), tracker_gps_scan_data, 4 );
            memcpyr( ( uint8_t *)( &cur_lat ), tracker_gps_scan_data + 4, 4 );

            if ( last_lat == 0 && last_lon == 0 )
            {
                /* First fix — always save, set baseline */
                last_lat = cur_lat;
                last_lon = cur_lon;
                moved = true;
            }
            else
            {
                int32_t dlat = cur_lat - last_lat;
                int32_t dlon = cur_lon - last_lon;
                if ( dlat < 0 ) dlat = -dlat;
                if ( dlon < 0 ) dlon = -dlon;
                /* ~25m box at 27.8°N: 1 unit lat≈0.11m, 1 unit lon≈0.098m */
                moved = ( dlat > 225 || dlon > 255 );
                if ( moved )
                {
                    last_lat = cur_lat;
                    last_lon = cur_lon;
                }
            }
        }

        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xBE;
        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xEF;
        /* v25: Pure motion gate — only cache entries that represent movement.
         * Stationary device = empty cache = consumer finds nothing to drain.
         * User presses and turbo always bypass the gate. */
        if ( moved || event_state == TRACKER_STATE_BIT8_USER || turbo_active )
        {
            tracker_cache_save( tracker_scan_data_temp, tracker_scan_temp_len );
            send_ok = true;
            last_tx_time = hal_rtc_get_time_s( );
        }
        if( send_ok ) tracker_gps_scan_len = 0;
    }
    else if( tracker_wifi_scan_len )
    {
        if( tracker_acc_en )
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_WIFI_SEN_ACC_BAT;
        }
        else
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_WIFI_SEN_BAT;
        }

        tracker_scan_data_temp[1] = event_state;
        tracker_scan_data_temp[2] = battery;
        memcpyr( tracker_scan_data_temp + 3, ( uint8_t *)( &temp ), 2 );
        memcpyr( tracker_scan_data_temp + 5, ( uint8_t *)( &light ), 2 );
        tracker_scan_temp_len += 7;

        if( tracker_acc_en )
        {
            memcpyr( tracker_scan_data_temp + 7, ( uint8_t *)( &ax ), 2 );
            memcpyr( tracker_scan_data_temp + 9, ( uint8_t *)( &ay ), 2 );
            memcpyr( tracker_scan_data_temp + 11, ( uint8_t *)( &az ), 2 );
            tracker_scan_temp_len += 6;
        }

        tracker_scan_data_temp[tracker_scan_temp_len] = tracker_wifi_scan_len / 7;
        tracker_scan_temp_len += 1;

        memcpy( tracker_scan_data_temp + tracker_scan_temp_len, tracker_wifi_scan_data, tracker_wifi_scan_len );
        tracker_scan_temp_len += tracker_wifi_scan_len;

        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xBE;
        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xEF;

        tracker_cache_save( tracker_scan_data_temp, tracker_scan_temp_len );
        send_ok = true;
        if( send_ok ) tracker_wifi_scan_len = 0;
    }
    else if( tracker_ble_scan_len )
    {
        if( tracker_acc_en )
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_BLE_SEN_ACC_BAT;
        }
        else
        {
            tracker_scan_data_temp[0] = DATA_ID_UP_PACKET_BLE_SEN_BAT;
        }
        
        tracker_scan_data_temp[1] = event_state;
        tracker_scan_data_temp[2] = battery;
        memcpyr( tracker_scan_data_temp + 3, ( uint8_t *)( &temp ), 2 );
        memcpyr( tracker_scan_data_temp + 5, ( uint8_t *)( &light ), 2 );
        tracker_scan_temp_len += 7;

        if( tracker_acc_en )
        {
            memcpyr( tracker_scan_data_temp + 7, ( uint8_t *)( &ax ), 2 );
            memcpyr( tracker_scan_data_temp + 9, ( uint8_t *)( &ay ), 2 );
            memcpyr( tracker_scan_data_temp + 11, ( uint8_t *)( &az ), 2 );
            tracker_scan_temp_len += 6;
        }

        tracker_scan_data_temp[tracker_scan_temp_len] = tracker_ble_scan_len / 7;
        tracker_scan_temp_len += 1;

        memcpy( tracker_scan_data_temp + tracker_scan_temp_len, tracker_ble_scan_data, tracker_ble_scan_len );
        tracker_scan_temp_len += tracker_ble_scan_len;

        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xBE;
        tracker_scan_data_temp[tracker_scan_temp_len++] = 0xEF;

        tracker_cache_save( tracker_scan_data_temp, tracker_scan_temp_len );
        send_ok = true;
        if( send_ok ) tracker_ble_scan_len = 0;
    }

    if( send_ok ) scan_result_num -= 1;
    if( scan_result_num )
    {
        schedule_producer( LORWAN_SEND_INTERVAL_MIN );
        HAL_DBG_TRACE_PRINTF( "next send, new alarm %d s\n\n", LORWAN_SEND_INTERVAL_MIN );
    }
    else
    {
        tracker_scan_status = 0;

        /* v22: Beep feedback after user-triggered or turbo scans.
         * Uses direct PWM control for simple multi-beep patterns.
         * Safe to block here — scan is done, next alarm not yet set. */
        if ( event_state == TRACKER_STATE_BIT8_USER )
        {
            if ( gps_had_fix )
            {
                /* GPS fix — 3 short beeps */
                hal_pwm_init( 2000 );
                for ( uint8_t i = 0; i < 3; i++ )
                {
                    hal_beep_on( );
                    hal_mcu_wait_ms( 80 );
                    hal_beep_off( );
                    if ( i < 2 ) hal_mcu_wait_ms( 120 );
                }
                hal_pwm_deinit( );
            }
            else
            {
                /* No GPS fix — 2 long beeps */
                hal_pwm_init( 2000 );
                for ( uint8_t i = 0; i < 2; i++ )
                {
                    hal_beep_on( );
                    hal_mcu_wait_ms( 500 );
                    hal_beep_off( );
                    if ( i < 1 ) hal_mcu_wait_ms( 200 );
                }
                hal_pwm_deinit( );
            }
        }
        else if ( turbo_active )
        {
            /* Turbo scan complete — 1 short beep */
            hal_pwm_init( 2000 );
            hal_beep_on( );
            hal_mcu_wait_ms( 80 );
            hal_beep_off( );
            hal_pwm_deinit( );
        }
        event_state = 0;  /* clear after every scan */

        /* Producer schedules its own next scan — never kicks consumer.
         * Consumer drains on its own independent timer. */
        {
            int32_t next_delay = tracker_periodic_interval - ( hal_rtc_get_time_s( ) - tracker_scan_begin );
            schedule_producer( next_delay > 0 ? next_delay : 1 );
        }
    }
}

static void app_tracker_scan_process( void )
{
    int32_t next_delay = 0;
    static uint8_t last_status = 0xFF;  /* watchdog: detect stuck state */

    /* Scan stall watchdog: if status hasn't changed for 3 alarm ticks,
     * force-reset to idle. Prevents GNSS/driver hangs from freezing device. */
    if( tracker_scan_status == last_status )
    {
        if( ++scan_stall_count >= 3 )
        {
            tracker_scan_status = 0;
            scan_stall_count = 0;
            scan_result_num = 0;
        }
    }
    else
    {
        scan_stall_count = 0;
    }
    last_status = tracker_scan_status;

    scan_result = false;

    if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_GNSS_ONLY ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( gnss_scan_duration );
            HAL_DBG_TRACE_PRINTF( "gnss begin, new alarm %d s\n\n", gnss_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            gps_scan_start_time = hal_rtc_get_time_s( );
            app_tracker_gnss_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            uint32_t gps_elapsed = hal_rtc_get_time_s( ) - gps_scan_start_time;
            if( gps_elapsed >= 30 )
            {
                /* Hard timeout — GPS didn't get a fix in 30s */
                app_tracker_gnss_scan_end( );
                tracker_scan_status = 0xff;
                schedule_producer( 1 );
                /* Distinctive 1s beep — timeout feedback */
                hal_pwm_init( 2000 );
                hal_beep_on( );
                hal_mcu_wait_ms( 500 );
                hal_beep_off( );
                hal_pwm_deinit( );
            }
            else
            {
            if ( event_state == TRACKER_STATE_BIT8_USER || turbo_active ) { next_delay = 1; } else { next_delay = tracker_periodic_interval - gnss_scan_duration; }
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "gnss end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_gnss_scan_end( );
            tracker_scan_status = 0xff;
            }
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_WIFI_ONLY ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( wifi_scan_duration );
            HAL_DBG_TRACE_PRINTF( "wifi begin, new alarm %d s\n\n", wifi_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_wifi_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            next_delay = tracker_periodic_interval - wifi_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "wifi end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_wifi_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_WIFI_GNSS ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( wifi_scan_duration );
            HAL_DBG_TRACE_PRINTF( "wifi begin, new alarm %d s\n\n", wifi_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_wifi_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            app_tracker_wifi_scan_end( );
            if( scan_result )
            {
                next_delay = tracker_periodic_interval - wifi_scan_duration;
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "wifi end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( gnss_scan_duration );
                HAL_DBG_TRACE_PRINTF( "wifi end\r\ngnss begin, new alarm %d s\n\n", gnss_scan_duration );
                tracker_scan_status = 2;
                app_tracker_gnss_scan_begin( );
            }          
        }
        else if( tracker_scan_status == 2 )
        {
            next_delay = tracker_periodic_interval - wifi_scan_duration - gnss_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "gnss end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_gnss_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_GNSS_WIFI ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( gnss_scan_duration );
            HAL_DBG_TRACE_PRINTF( "gnss begin, new alarm %d s\n\n", gnss_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_gnss_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            app_tracker_gnss_scan_end( );
            if( scan_result )
            {
                if ( event_state == TRACKER_STATE_BIT8_USER || turbo_active ) { next_delay = 1; } else { next_delay = tracker_periodic_interval - gnss_scan_duration; }
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "gnss end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( wifi_scan_duration );
                HAL_DBG_TRACE_PRINTF( "gnss end\r\nwifi begin, new alarm %d s\n\n", wifi_scan_duration );
                app_tracker_wifi_scan_begin( );
                tracker_scan_status = 2;
            }
        }
        else if( tracker_scan_status == 2 )
        {
            next_delay = tracker_periodic_interval - gnss_scan_duration - wifi_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "wifi end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_wifi_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_BLE_ONLY ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( ble_scan_duration );
            HAL_DBG_TRACE_PRINTF( "ble begin, new alarm %d s\n\n", ble_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_ble_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            next_delay = tracker_periodic_interval - ble_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "ble end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_ble_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_BLE_WIFI ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( ble_scan_duration );
            HAL_DBG_TRACE_PRINTF( "ble begin, new alarm %d s\n\n", ble_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_ble_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            app_tracker_ble_scan_end( );
            if( scan_result )
            {
                next_delay = tracker_periodic_interval - ble_scan_duration;
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "ble end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( wifi_scan_duration );
                HAL_DBG_TRACE_PRINTF( "ble end\r\nwifi begin, new alarm %d s\n\n", wifi_scan_duration );
                app_tracker_wifi_scan_begin( );
                tracker_scan_status = 2;
            }
        }
        else if( tracker_scan_status == 2 )
        {
            next_delay = tracker_periodic_interval - ble_scan_duration - wifi_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "wifi end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_wifi_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_BLE_GNSS ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( ble_scan_duration );
            HAL_DBG_TRACE_PRINTF( "ble begin, new alarm %d s\n\n", ble_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_ble_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            app_tracker_ble_scan_end( );
            if( scan_result )
            {
                next_delay = tracker_periodic_interval - ble_scan_duration;
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "ble end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( gnss_scan_duration );
                HAL_DBG_TRACE_PRINTF( "ble end\r\ngnss begin, new alarm %d s\n\n", gnss_scan_duration );
                app_tracker_gnss_scan_begin( );
                tracker_scan_status = 2;
            }
        }
        else if( tracker_scan_status == 2 )
        {
            next_delay = tracker_periodic_interval - ble_scan_duration - gnss_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "gnss end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_gnss_scan_end( );
            tracker_scan_status = 0xff;
        }
    }
    else if(( scan_result == false ) && ( tracker_scan_type == TRACKER_SCAN_BLE_WIFI_GNSS ))
    {
        if( tracker_scan_status == 0 )
        {
            schedule_producer( ble_scan_duration );
            HAL_DBG_TRACE_PRINTF( "ble begin, new alarm %d s\n\n", ble_scan_duration );
            tracker_scan_begin = hal_rtc_get_time_s( );
            app_tracker_ble_scan_begin( );
            tracker_scan_status = 1;
        }
        else if( tracker_scan_status == 1 )
        {
            app_tracker_ble_scan_end( );
            if( scan_result )
            {
                next_delay = tracker_periodic_interval - ble_scan_duration;
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "ble end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( wifi_scan_duration );
                HAL_DBG_TRACE_PRINTF( "ble end\r\nwifi begin, new alarm %d s\n\n", wifi_scan_duration );
                app_tracker_wifi_scan_begin( );
                tracker_scan_status = 2;
            }
        }
        else if( tracker_scan_status == 2 )
        {
            app_tracker_wifi_scan_end( );
            if( scan_result )
            {
                next_delay = tracker_periodic_interval - ble_scan_duration - wifi_scan_duration;
                schedule_producer( next_delay > 0 ? next_delay : 1 );
                HAL_DBG_TRACE_PRINTF( "wifi end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
                tracker_scan_status = 0xff;
            }
            else
            {
                schedule_producer( gnss_scan_duration );
                HAL_DBG_TRACE_PRINTF( "wifi end\r\ngnss begin, new alarm %d s\n\n", gnss_scan_duration );
                tracker_scan_status = 3;
                app_tracker_gnss_scan_begin( );
            }
        }
        else if( tracker_scan_status == 3 )
        {
            next_delay = tracker_periodic_interval - ble_scan_duration - wifi_scan_duration - gnss_scan_duration;
            schedule_producer( next_delay > 0 ? next_delay : 1 );
            HAL_DBG_TRACE_PRINTF( "gnss end, new alarm %d s\n\n", next_delay > 0 ? next_delay : 1 );
            app_tracker_gnss_scan_end( );
            tracker_scan_status = 0xff;
        }
    }

    if( tracker_scan_status == 0xff )
    {
        app_tracker_scan_result_send( );
    }
}

bool app_send_frame( const uint8_t* buffer, const uint8_t length, bool tx_confirmed, bool emergency )
{
    uint8_t tx_max_payload;
    int32_t duty_cycle;

    /* Check if duty cycle is available */
    ASSERT_SMTC_MODEM_RC( smtc_modem_get_duty_cycle_status( &duty_cycle ) );
    if( duty_cycle < 0 )
    {
        HAL_DBG_TRACE_WARNING( "Duty-cycle limitation - next possible uplink in %d ms \n\n", duty_cycle );
        return false;
    }

    ASSERT_SMTC_MODEM_RC( smtc_modem_get_next_tx_max_payload( stack_id, &tx_max_payload ) );
    if( length > tx_max_payload )
    {
        HAL_DBG_TRACE_WARNING( "Not enough space in buffer - send empty uplink to flush MAC commands \n" );
        ASSERT_SMTC_MODEM_RC( smtc_modem_request_empty_uplink( stack_id, true, LORAWAN_APP_PORT, tx_confirmed ));
        return false;
    }
    else
    {
        HAL_DBG_TRACE_INFO( "Request uplink\n" );
        if( emergency )
        {
            ASSERT_SMTC_MODEM_RC ( smtc_modem_request_emergency_uplink( stack_id, LORAWAN_APP_PORT, tx_confirmed, buffer, length ));
        }
        else
        {
            ASSERT_SMTC_MODEM_RC( smtc_modem_request_uplink( stack_id, LORAWAN_APP_PORT, tx_confirmed, buffer, length ));
        }
        return true;
    }
}

void app_tracker_new_run( uint8_t event )
{
    /* If a scan is already running, ignore additional presses. */
    if( tracker_scan_status != 0 )
    {
        return;
    }
    event_state = event;
    if( tracker_scan_status == 0 ) // Not tracking — flag for main loop
    {
        producer_pending = true;
        hal_sleep_exit( );  /* wake CPU so main loop picks up the flag */
    }
}

void app_radio_set_sleep( void )
{
    lr11xx_system_sleep_cfg_t radio_sleep_cfg;

    radio_sleep_cfg.is_warm_start  = true;
    radio_sleep_cfg.is_rtc_timeout = false;

    if( lr11xx_system_cfg_lfclk( modem_radio->ral.context, LR11XX_SYSTEM_LFCLK_RC, true ) != LR11XX_STATUS_OK )
    {
        HAL_DBG_TRACE_ERROR( "Failed to set LF clock\n" );
    }
    if( lr11xx_system_set_sleep( modem_radio->ral.context, radio_sleep_cfg, 0 ) != LR11XX_STATUS_OK )
    {
        HAL_DBG_TRACE_ERROR( "Failed to set the radio to sleep\n" );
    }
}

void app_lora_stack_suspend( void )
{
    smtc_modem_alarm_clear_timer( );
    smtc_modem_leave_network( stack_id );
    smtc_modem_suspend_radio_communications( true );
}

/* ── v22: Scan interval & turbo mode helpers ─────────────────────── */

void app_tracker_set_interval( uint32_t minutes )
{
    if ( minutes > 0 )
    {
        tracker_periodic_interval = minutes;
    }
}

uint32_t app_tracker_get_interval( void )
{
    return tracker_periodic_interval;
}

void app_tracker_force_drain( void )
{
    force_drain_pending = true;
    schedule_consumer( 1 );
}

void app_tracker_turbo_toggle( void )
{
    if ( turbo_active )
    {
        /* Exit turbo — restore saved interval */
        tracker_periodic_interval = saved_periodic_interval;
        turbo_active = false;
        /* Kick drain, but don't interrupt a running scan */
        if( tracker_scan_status != 1 )
        {
            schedule_producer( 1 );
        }
    }
    else
    {
        /* Enter turbo — save current interval, switch to ~1 minute cycle. */
        saved_periodic_interval = tracker_periodic_interval;
        tracker_periodic_interval = 60;
        turbo_active = true;
        /* Kick off first turbo scan immediately (2s delay for beep to finish) */
        schedule_producer( 2 );
    }
}

bool app_tracker_is_turbo( void )
{
    return turbo_active;
}

/* --- EOF ------------------------------------------------------------------ */
