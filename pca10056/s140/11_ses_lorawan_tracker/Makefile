# Makefile for T1000-E factory firmware build
# Uses PlatformIO ARM GCC toolchain (NOT SES licensed compiler)

PROJECT_DIR := C:/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e
SKD_DIR := C:/nRF5_SDK_17.1.0_ddde560
BUILD_DIR := $(PROJECT_DIR)/pca10056/s140/11_ses_lorawan_tracker/Output/Debug
OBJ_DIR := $(BUILD_DIR)/Obj/t1000_e_dev_kit_pca10056
EXE_DIR := $(BUILD_DIR)/Exe

GCC := C:/Users/ViV/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc
GXX := C:/Users/ViV/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-g++
OBJCOPY := C:/Users/ViV/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-objcopy
SIZE := C:/Users/ViV/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-size
UF2CONV := python3 C:/Users/ViV/uf2conv.py

# SoftDevice
SD_HEX := $(SKD_DIR)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex

# Target: nRF52840 with SoftDevice s140
MCU := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
DEFINES := -DNRF52840_XXAA -DNRF_SD_BLE_API_VERSION=7 -DS140 -DSOFTDEVICE_PRESENT \
           -DBOARD_PCA10056 -DCONFIG_GPIO_AS_PINRESET -DFLOAT_ABI_HARD \
           -DINITIALIZE_USER_SECTIONS -DNO_VTOR_CONFIG \
           -DAPP_TIMER_V2 -DAPP_TIMER_V2_RTC1_ENABLED \
           -DLR11XX -DLR11XX_TRANSCEIVER -DENABLE_MODEM_GNSS_FEATURE \
           -DADD_SMTC_ALC_SYNC -DADD_SMTC_FILE_UPLOAD -DADD_SMTC_STREAM \
           -DAPP_TRACKER -DRP2_103 -DSMTC_MULTICAST \
           -DTASK_EXTENDED_1 -DTASK_EXTENDED_2 \
           -DHAL_DBG_TRACE=1 -DMODEM_HAL_DBG_TRACE=1 \
           -DREGION_AS_923 -DREGION_AU_915 -DREGION_EU_868 \
           -DREGION_IN_865 -DREGION_KR_920 -DREGION_RU_864 -DREGION_US_915

# Include paths (relative to project dir)
INC_BASE := -I$(SKD_DIR)/components -I$(SKD_DIR)/components/ble/ble_advertising \
            -I$(SKD_DIR)/components/ble/ble_dtm -I$(SKD_DIR)/components/ble/ble_link_ctx_manager \
            -I$(SKD_DIR)/components/ble/ble_racp \
            -I$(SKD_DIR)/components/ble/ble_services/ble_ancs_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_ans_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_bas \
            -I$(SKD_DIR)/components/ble/ble_services/ble_bas_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_cscs \
            -I$(SKD_DIR)/components/ble/ble_services/ble_cts_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_dis \
            -I$(SKD_DIR)/components/ble/ble_services/ble_gls \
            -I$(SKD_DIR)/components/ble/ble_services/ble_hids \
            -I$(SKD_DIR)/components/ble/ble_services/ble_hrs \
            -I$(SKD_DIR)/components/ble/ble_services/ble_hrs_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_hts \
            -I$(SKD_DIR)/components/ble/ble_services/ble_ias \
            -I$(SKD_DIR)/components/ble/ble_services/ble_ias_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_lbs \
            -I$(SKD_DIR)/components/ble/ble_services/ble_lbs_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_lls \
            -I$(SKD_DIR)/components/ble/ble_services/ble_nus_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_rscs \
            -I$(SKD_DIR)/components/ble/ble_services/ble_rscs_c \
            -I$(SKD_DIR)/components/ble/ble_services/ble_tps \
            -I$(SKD_DIR)/components/ble/common -I$(SKD_DIR)/components/ble/nrf_ble_gatt \
            -I$(SKD_DIR)/components/ble/nrf_ble_qwr -I$(SKD_DIR)/components/ble/nrf_ble_scan \
            -I$(SKD_DIR)/components/ble/peer_manager -I$(SKD_DIR)/components/boards \
            -I$(SKD_DIR)/components/libraries/atomic -I$(SKD_DIR)/components/libraries/atomic_fifo \
            -I$(SKD_DIR)/components/libraries/atomic_flags -I$(SKD_DIR)/components/libraries/balloc \
            -I$(SKD_DIR)/components/libraries/bsp -I$(SKD_DIR)/components/libraries/delay \
            -I$(SKD_DIR)/components/libraries/experimental_section_vars \
            -I$(SKD_DIR)/components/libraries/fifo -I$(SKD_DIR)/components/libraries/fds \
            -I$(SKD_DIR)/components/libraries/fstorage -I$(SKD_DIR)/components/libraries/hardfault \
            -I$(SKD_DIR)/components/libraries/log -I$(SKD_DIR)/components/libraries/log/src \
            -I$(SKD_DIR)/components/libraries/memobj -I$(SKD_DIR)/components/libraries/mutex \
            -I$(SKD_DIR)/components/libraries/pwm -I$(SKD_DIR)/components/libraries/pwr_mgmt \
            -I$(SKD_DIR)/components/libraries/queue -I$(SKD_DIR)/components/libraries/ringbuf \
            -I$(SKD_DIR)/components/libraries/sortlist -I$(SKD_DIR)/components/libraries/strerror \
            -I$(SKD_DIR)/components/libraries/timer -I$(SKD_DIR)/components/libraries/usbd \
            -I$(SKD_DIR)/components/libraries/usbd/class/cdc \
            -I$(SKD_DIR)/components/libraries/usbd/class/cdc/acm \
            -I$(SKD_DIR)/components/libraries/util -I$(SKD_DIR)/components/softdevice/common \
            -I$(SKD_DIR)/components/softdevice/s140/headers \
            -I$(SKD_DIR)/components/softdevice/s140/headers/nrf52 \
            -I$(SKD_DIR)/components/toolchain/cmsis/include \
            -I$(PROJECT_DIR) \
            -I$(SKD_DIR)/external/fprintf -I$(SKD_DIR)/external/utf_converter \
            -I$(SKD_DIR)/integration/nrfx -I$(SKD_DIR)/integration/nrfx/legacy \
            -I$(SKD_DIR)/modules/nrfx -I$(SKD_DIR)/modules/nrfx/drivers/include \
            -I$(SKD_DIR)/modules/nrfx/hal -I$(SKD_DIR)/modules/nrfx/mdk \
            -I$(PROJECT_DIR)/smtc_hal -I$(PROJECT_DIR)/smtc_hal/inc \
            -I$(PROJECT_DIR)/t1000_e/libraries/minmea -I$(PROJECT_DIR)/t1000_e/libraries/uart \
            -I$(PROJECT_DIR)/t1000_e/interface \
            -I$(PROJECT_DIR)/t1000_e/LR11XX/common/inc \
            -I$(PROJECT_DIR)/t1000_e/LR11XX/radio_drivers_hal \
            -I$(PROJECT_DIR)/t1000_e/LR11XX/smtc_lr11xx_board \
            -I$(PROJECT_DIR)/t1000_e/LR11XX/smtc_shield_lr11xx/common/inc \
            -I$(PROJECT_DIR)/t1000_e/peripherals/inc \
            -I$(PROJECT_DIR)/lora_basics_modem \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_api \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_hal \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/device_management \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lorawan_api \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac/src/lr1mac_class_b \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac/src/lr1mac_class_c \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac/src/services \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/lr1mac/src/smtc_real/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_config \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_core \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_services \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_supervisor \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_drivers/lr11xx_driver/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_planner/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/lr11xx_crypto_engine \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/smtc_secure_element \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/soft_secure_element \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/headers \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/alc_sync \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/almanac_update \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/file_upload \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/stream \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_ral/src \
            -I$(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_ralf/src \
            -I$(PROJECT_DIR)/apps/common \
            -I$(PROJECT_DIR)/t1000_e/tracker/inc \
            -I$(PROJECT_DIR)/t1000_e/ble_service/ble_nus \
            -I$(BUILD_DIR)/../config_sd

# SDK config header
INC := $(INC_BASE) -include $(BUILD_DIR)/../config_sd/sdk_config.h

CFLAGS := $(MCU) $(DEFINES) $(INC) \
          -O0 -g3 -ffunction-sections -fdata-sections \
          -std=gnu11 -Wall \
          -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-parameter \
          -Wno-pointer-sign -Wno-unused-function -Wno-missing-braces

LDFLAGS := $(MCU) -T $(BUILD_DIR)/linker.ld \
           -Wl,--gc-sections -Wl,-Map=$(EXE_DIR)/t1000_e_dev_kit_pca10056.map \
           -specs=nano.specs -lc -lnosys \
           -L$(SKD_DIR)/components/toolchain/gcc \
           -Wl,--no-wchar-size-warning

# Source files
SRCS := \
    $(SKD_DIR)/components/libraries/log/src/nrf_log_frontend.c \
    $(SKD_DIR)/components/libraries/log/src/nrf_log_str_formatter.c \
    $(SKD_DIR)/components/libraries/util/app_error.c \
    $(SKD_DIR)/components/libraries/util/app_error_handler_gcc.c \
    $(SKD_DIR)/components/libraries/util/app_error_weak.c \
    $(SKD_DIR)/components/libraries/fifo/app_fifo.c \
    $(SKD_DIR)/components/libraries/util/app_util_platform.c \
    $(SKD_DIR)/components/libraries/timer/drv_rtc.c \
    $(SKD_DIR)/components/libraries/util/nrf_assert.c \
    $(SKD_DIR)/components/libraries/atomic/nrf_atomic.c \
    $(SKD_DIR)/components/libraries/balloc/nrf_balloc.c \
    $(SKD_DIR)/external/fprintf/nrf_fprintf.c \
    $(SKD_DIR)/external/fprintf/nrf_fprintf_format.c \
    $(SKD_DIR)/components/libraries/fstorage/nrf_fstorage.c \
    $(SKD_DIR)/components/libraries/fstorage/nrf_fstorage_nvmc.c \
    $(SKD_DIR)/components/libraries/fstorage/nrf_fstorage_sd.c \
    $(SKD_DIR)/components/libraries/memobj/nrf_memobj.c \
    $(SKD_DIR)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
    $(SKD_DIR)/components/libraries/queue/nrf_queue.c \
    $(SKD_DIR)/components/libraries/ringbuf/nrf_ringbuf.c \
    $(SKD_DIR)/components/libraries/sortlist/nrf_sortlist.c \
    $(SKD_DIR)/components/libraries/strerror/nrf_strerror.c \
    $(SKD_DIR)/components/libraries/atomic_fifo/nrf_atfifo.c \
    $(SKD_DIR)/components/libraries/atomic_flags/nrf_atflags.c \
    $(SKD_DIR)/components/libraries/experimental_section_vars/nrf_section_iter.c \
    $(SKD_DIR)/components/libraries/timer/app_timer2.c \
    $(SKD_DIR)/components/softdevice/common/nrf_sdh.c \
    $(SKD_DIR)/components/softdevice/common/nrf_sdh_ble.c \
    $(SKD_DIR)/components/softdevice/common/nrf_sdh_soc.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_clock.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_power.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_ppi.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_rng.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_twi.c \
    $(SKD_DIR)/integration/nrfx/legacy/nrf_drv_uart.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_clock.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_gpiote.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_power.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_ppi.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_pwm.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_rng.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_rtc.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_spim.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_timer.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_twi.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_twim.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_uart.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_uarte.c \
    $(SKD_DIR)/modules/nrfx/drivers/src/nrfx_wdt.c \
    $(SKD_DIR)/components/libraries/fds/fds.c \
    $(SKD_DIR)/components/libraries/pwm/app_pwm.c \
    $(SKD_DIR)/components/libraries/usbd/app_usbd.c \
    $(SKD_DIR)/components/libraries/usbd/app_usbd_core.c \
    $(SKD_DIR)/components/libraries/usbd/app_usbd_serial_num.c \
    $(SKD_DIR)/components/libraries/usbd/app_usbd_string_desc.c \
    $(SKD_DIR)/components/libraries/usbd/class/cdc/acm/app_usbd_cdc_acm.c \
    $(SKD_DIR)/components/ble/common/ble_advdata.c \
    $(SKD_DIR)/components/ble/common/ble_conn_params.c \
    $(SKD_DIR)/components/ble/common/ble_conn_state.c \
    $(SKD_DIR)/components/ble/common/ble_srv_common.c \
    $(SKD_DIR)/components/ble/nrf_ble_gatt/nrf_ble_gatt.c \
    $(SKD_DIR)/components/ble/nrf_ble_qwr/nrf_ble_qwr.c \
    $(SKD_DIR)/components/ble/nrf_ble_scan/nrf_ble_scan.c \
    $(SKD_DIR)/components/ble/ble_advertising/ble_advertising.c \
    $(SKD_DIR)/components/ble/ble_link_ctx_manager/ble_link_ctx_manager.c \
    $(SKD_DIR)/components/ble/peer_manager/auth_status_tracker.c \
    $(SKD_DIR)/components/ble/peer_manager/gatt_cache_manager.c \
    $(SKD_DIR)/components/ble/peer_manager/gatts_cache_manager.c \
    $(SKD_DIR)/components/ble/peer_manager/id_manager.c \
    $(SKD_DIR)/external/utf_converter/utf.c \
    $(SKD_DIR)/components/boards/boards.c \
    $(PROJECT_DIR)/smtc_hal/smtc_modem_hal.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_adc.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_flash.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_gpio.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_i2c.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_lp_time.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_mcu.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_pwm.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_rng.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_rtc.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_spi.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_trace.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_uart.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_usb_cdc.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_usb_time.c \
    $(PROJECT_DIR)/smtc_hal/src/smtc_hal_watchdog.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/ag3335.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/beep_music.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/qma6100p.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/sensor.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/wifi_helpers.c \
    $(PROJECT_DIR)/t1000_e/peripherals/src/wifi_scan.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_at.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_at_command.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_at_fds_datas.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_beep.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_ble_all.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_board.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_button.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_config_param.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_led.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_lora_packet.c \
    $(PROJECT_DIR)/t1000_e/tracker/src/app_user_timer.c \
    $(PROJECT_DIR)/t1000_e/libraries/minmea/minmea.c \
    $(PROJECT_DIR)/t1000_e/libraries/uart/app_uart_fifo.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/common/src/ral_lr11xx_bsp.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/common/src/smtc_board_lr11xx.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/radio_drivers_hal/lr11xx_hal.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/smtc_shield_lr11xx/common/src/smtc_shield_lr11x0_common.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/smtc_shield_lr11xx/common/src/smtc_shield_lr11xx_common.c \
    $(PROJECT_DIR)/t1000_e/LR11XX/smtc_shield_lr11xx/LR1110/LR1110MB1DxS/smtc_shield_lr1110mb1dxs.c \
    $(PROJECT_DIR)/t1000_e/ble_service/ble_nus/app_ble_nus.c \
    $(PROJECT_DIR)/apps/examples/11_lorawan_tracker/main_lorawan_tracker.c \
    $(PROJECT_DIR)/apps/common/apps_modem_common_ex.c

# Lora basics modem source files
SRCS += \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_hal/smtc_modem_hal.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_planner/src/radio_planner.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_planner/src/radio_planner_hal.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_ral/src/ral_lr11xx.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_ralf/src/ralf_lr11xx.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_drivers/lr11xx_driver/src/lr11xx_radio.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_drivers/lr11xx_driver/src/lr11xx_regmem.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_drivers/lr11xx_driver/src/lr11xx_system.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/radio_drivers/lr11xx_driver/src/lr11xx_wifi.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/smtc_modem_crypto.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/soft_secure_element/aes.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/soft_secure_element/cmac.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_crypto/soft_secure_element/soft_se.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/alc_sync/alc_sync.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/almanac_update/almanac_update.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/file_upload/file_upload.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/stream/rose.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/smtc_modem_services/src/stream/stream.c

# And more modem core files
SRCS += \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_core/mdm_core.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_services/mdm_services.c \
    $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core/modem_supervisor/mdm_supervisor.c

# We need to find all the modem core source files
SRC_DIRS := $(PROJECT_DIR)/lora_basics_modem/smtc_modem_core

OBJS := $(patsubst $(SKD_DIR)/%,$(OBJ_DIR)/%,$(patsubst $(PROJECT_DIR)/%,$(OBJ_DIR)/%,$(SRCS:.c=.o)))
OBJS := $(subst :,_,$(OBJS))

.PHONY: all clean uf2

all: $(EXE_DIR)/t1000_e_dev_kit_pca10056.hex

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(GCC) $(CFLAGS) -c $< -o $@

# Build the elf
$(EXE_DIR)/t1000_e_dev_kit_pca10056.elf: $(OBJS) | $(EXE_DIR)
	$(GCC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(EXE_DIR)/t1000_e_dev_kit_pca10056.hex: $(EXE_DIR)/t1000_e_dev_kit_pca10056.elf
	$(OBJCOPY) -O ihex $< $@

$(EXE_DIR) $(OBJ_DIR):
	mkdir -p $@

uf2: $(EXE_DIR)/t1000_e_dev_kit_pca10056.hex
	$(UF2CONV) -f 0xADA52840 -b 0x27000 -c -o $(EXE_DIR)/t1000_e_dev_kit_factory.uf2 $<

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(EXE_DIR)/*.elf $(EXE_DIR)/*.hex $(EXE_DIR)/*.map $(EXE_DIR)/*.uf2

-include $(OBJS:.o=.d)
