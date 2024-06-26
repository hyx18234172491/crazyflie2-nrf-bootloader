/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_hrs_eval_main main.c
 * @{
 * @ingroup ble_sdk_app_hrs_eval
 * @brief Main file for Heart Rate Service Sample Application for nRF51822 evaluation board
 *
 * This file contains the source code for a sample application using the Heart Rate service
 * (and also Battery and Device Information services) for the nRF51822 evaluation board (PCA10001).
 * This application uses the @ref ble_sdk_lib_conn_params module.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf51_bitfields.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_bas.h"
#include "ble_hrs.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "nrf_gpio.h"
#include "ble_debug_assert_handler.h"

#include "bitcraze_uuids.h"

#include "ble_crazyflies.h"
#include "timeslot.h"
#include "bootloader.h"


#if BLE
#if S110==0
#error S110 shall be used when using BLE
#endif
#endif

//#define DEVICE_NAME                          "Crazyflie"                              /**< Name of device. Will be included in the advertising data. */
#define DEVICE_NAME                          "Crazyflie Loader"
#define MANUFACTURER_NAME                    "Bitcraze"                     /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                     400                                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 250 ms (hopefully 2 sb slots between ADV)). */
#define APP_ADV_TIMEOUT_IN_SECONDS           0                                       /**< The advertising timeout in units of seconds. */

#define MIN_CONN_INTERVAL                    MSEC_TO_UNITS(20, UNIT_1_25_MS)          /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL                    MSEC_TO_UNITS(40, UNIT_1_25_MS)         /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                        0                                         /**< Slave latency. */
#define CONN_SUP_TIMEOUT                     MSEC_TO_UNITS(4000, UNIT_10_MS)           /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(5000, 0)/**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(5000, 0)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT         3                                         /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_TIMEOUT                    30                                        /**< Timeout for Pairing Request or Security Request (in seconds). */
#define SEC_PARAM_BOND                       1                                         /**< Perform bonding. */
#define SEC_PARAM_MITM                       0                                         /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES            BLE_GAP_IO_CAPS_NONE                      /**< No I/O capabilities. */
#define SEC_PARAM_OOB                        0                                         /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE               7                                         /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE               16                                        /**< Maximum encryption key size. */

#define DEAD_BEEF                            0xDEADBEEF                                /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static ble_gap_sec_params_t                  m_sec_params;                             /**< Security requirements for this application. */
static ble_gap_adv_params_t                  m_adv_params;                             /**< Parameters to be passed to the stack when starting advertising. */

static void ble_evt_dispatch(ble_evt_t * p_ble_evt);

static void sys_evt_dispatch(uint32_t sys_evt);


/*****************************************************************************
* Error Handling Functions
*****************************************************************************/


/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    // This call can be used for debug purposes during application development.
    // @note CAUTION: Activating this code will write the stack to flash on an error.
    //                This function should NOT be used in a final product.
    //                It is intended STRICTLY for development/debugging purposes.
    //                The flash write will happen EVEN if the radio is active, thus interrupting
    //                any communication.
    //                Use with care. Un-comment the line below to use.
    // ble_debug_assert_handler(error_code, line_num, p_file_name);

    // On assert, the system can only recover with a reset.
    //NVIC_SystemReset();

    while(1);
}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling a Bond Manager error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void bond_manager_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/*****************************************************************************
* Static Initialization Functions
*****************************************************************************/



/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    ble_uuid_t adv_uuids[] =
    {
        //{BLE_UUID_HEART_RATE_SERVICE,         BLE_UUID_TYPE_BLE},
        //{BLE_UUID_BATTERY_SERVICE,            BLE_UUID_TYPE_BLE},
        {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
    };

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags.size              = sizeof(flags);
    advdata.flags.p_data            = &flags;
    advdata.uuids_more_available.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_more_available.p_uuids  = adv_uuids;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);

    // Initialize advertising parameters (used when starting advertising)
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.p_peer_addr = NULL;                           // Undirected advertisement
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = APP_ADV_INTERVAL;
    m_adv_params.timeout     = APP_ADV_TIMEOUT_IN_SECONDS;
}


/**@brief Function for initializing the services that will be used by the application.
 *
 * @details Initialize the Heart Rate, Battery and Device Information services.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_dis_init_t dis_init;
    uint8_t bitcrazeUuidType;

    const ble_uuid128_t base_uuid128 =
      {
         { UUID_BITCRAZE_BASE }
      };

    // Initialize Bitcraze services
    err_code = sd_ble_uuid_vs_add(&base_uuid128, &bitcrazeUuidType);
    APP_ERROR_CHECK(err_code);

    ble_crazyflies_init(bitcrazeUuidType);

    // Initialize Device Information Service
    memset(&dis_init, 0, sizeof(dis_init));

    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, MANUFACTURER_NAME);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the security parameters.
 */
static void sec_params_init(void)
{
    m_sec_params.timeout      = SEC_PARAM_TIMEOUT;
    m_sec_params.bond         = SEC_PARAM_BOND;
    m_sec_params.mitm         = SEC_PARAM_MITM;
    m_sec_params.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    m_sec_params.oob          = SEC_PARAM_OOB;
    m_sec_params.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    m_sec_params.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
}


/**@brief Function for initializing the Connection Parameters module.
 */

static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    //cp_init.start_on_notify_cccd_handle    = m_hrs.hrm_handles.cccd_handle;
    cp_init.disconnect_on_fail             = true;
    cp_init.evt_handler                    = NULL;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
    ble_gap_addr_t addr;
    ble_enable_params_t enableParams;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, false);

    // Enable BLE stack
    enableParams.gatts_enable_params.service_changed = 1;
    sd_ble_enable(&enableParams);

    // The bootloader takes the next address compared to Crazyflie firmware
    sd_ble_gap_address_get(&addr);
    addr.addr[0]++;
    sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/*****************************************************************************
* Static Start Functions
*****************************************************************************/

/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);
}


/*****************************************************************************
* Static Event Handling Functions
*****************************************************************************/

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t        err_code;
    static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:

            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            //APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            // Since we are not in a connection and have not started advertising, store bonds
            //err_code = ble_bondmngr_bonded_centrals_store();
            //APP_ERROR_CHECK(err_code);

            //m_storage_in_progress = true;

            // @note Flash access may not be complete on return of this API. System attributes are now
            // stored to flash when they are updated to ensure flash access on disconnect does not
            // result in system powering off before data was successfully written.

            // Go to system-off mode, should not return from this function, wakeup will trigger
            // a reset.
            //system_off_mode_enter();
            advertising_start();
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                   BLE_GAP_SEC_STATUS_SUCCESS,
                                                   &m_sec_params);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISEMENT)
            {
                advertising_start();
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    //ble_bondmngr_on_ble_evt(p_ble_evt);
    // ble_leds_on_ble_evt(p_ble_evt);
    ble_crazyflies_on_ble_evt(p_ble_evt);

    ble_conn_params_on_ble_evt(p_ble_evt);

    on_ble_evt(p_ble_evt);
    // printf("");
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    timeslot_sd_evt_signal(sys_evt);
    bootloaderOnSdEvt(sys_evt);

    //pstorage_sys_event_handler(sys_evt);

    //uint32_t count;
    //uint32_t err_code;

    // Check if storage access is in progress.
    /*
    err_code = pstorage_access_status_get(&count);
    if ((err_code == NRF_SUCCESS) && (count == 0))
    {
        storage_access_complete_handler();
    }
    */
}


/*****************************************************************************
* Main Function
*****************************************************************************/

/**@brief Function for the application main entry.
 */
int ble_init(void)
{
    //uint32_t err_code;

    ble_stack_init();
    //bond_manager_init();

    // Initialize Bluetooth Stack parameters
    gap_params_init();
    advertising_init();
    services_init();
    conn_params_init();
    sec_params_init();

    // Start advertising
    advertising_start();
    timeslot_start();

    // Enter main loop
    //for (;;)
    //{
    //    // Switch to a low power state until an event is available for the application
    //    err_code = sd_app_evt_wait();
    //    APP_ERROR_CHECK(err_code);
    //}

}

/**
 * @}
 */
