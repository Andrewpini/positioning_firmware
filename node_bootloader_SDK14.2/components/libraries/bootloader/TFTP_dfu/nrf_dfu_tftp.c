/* Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
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

#include "nrf_dfu.h"
#include "nrf_dfu_transport.h"
#include "nrf_dfu_utils.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_dfu_settings.h"
#include "nrf_gpio.h"
#include "app_scheduler.h"
//#include "app_timer_appsh.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "boards.h"
#include "nrf_bootloader_info.h"
#include "nrf_dfu_req_handler.h"

//#include "loopback.h"
#include "w5500.h"
#include "socket.h"
#include "user_ethernet.h"
//#include "httpServer.h"
//#include "webpage.h"
#include "loopback.h"
#include "tftp.h"
#include "tftp_config.h"
//#include "ConfigData.h"
//#include "ConfigMessage.h"
#include "ethernet_dfu.h"

#define SCHED_MAX_EVENT_DATA_SIZE       MAX(APP_TIMER_SCHED_EVT_SIZE, 0)                        /**< Maximum size of scheduler events. */

#define SCHED_QUEUE_SIZE                20                                                      /**< Maximum number of events in the scheduler queue. */

#define APP_TIMER_PRESCALER             0                                                       /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                                       /**< Size of timer operation queues. */

#define FILE_NAME "node.bin"

uint8_t g_socket_rcv_buf[MAX_MTU_SIZE];
uint8_t g_op_mode = NORMAL_MODE;
uint32_t g_wirite_addr;

uint8_t targetIP[4] = {1, 1, 1, 1};
//uint32_t tcp_targetPort = 69;


// Weak function implementation

/** @brief Weak implemenation of nrf_dfu_check_enter.
 *
 * @note    This function must be overridden to enable entering DFU mode at will.
 *          Default behaviour is to enter DFU when BOOTLOADER_BUTTON is pressed.
 */
__WEAK bool nrf_dfu_enter_check(void)
{	
    if (NRF_POWER->GPREGRET == 0xAA)
    {
		NRF_POWER->GPREGRET = 0x00;
        return true;
    }
		
		if (NRF_POWER->GPREGRET == 0xBB)
    {
		NRF_POWER->GPREGRET = 0x00;
			
		while (nrf_gpio_pin_read(BOOTLOADER_BUTTON) != 0)
    {
    }
        return true;
    }

    if (s_dfu_settings.enter_buttonless_dfu == 1)
    {
        s_dfu_settings.enter_buttonless_dfu = 0;
        APP_ERROR_CHECK(nrf_dfu_settings_write(NULL));
        return true;
    }
    return false;
}

void set_target_ip()
{
	uint32_t* p_server_ip = (uint32_t*)0xFE000;
	
	for(uint16_t i=0; i<4096; i++)
  {
    if (p_server_ip >= (uint32_t*)0xFF000)
    {
			while(true)
			{
			}
    } 
    else if (*p_server_ip == 0xFFFFFFFF)
    {
			p_server_ip -= 1;
      break;
    } 
    else
    {
      p_server_ip += 1;
    }
  }
	
	uint32_t server_ip = *p_server_ip;
	
	targetIP[0] = ((uint8_t*)&server_ip)[0];
	targetIP[1] = ((uint8_t*)&server_ip)[1];
	targetIP[2] = ((uint8_t*)&server_ip)[2];
	targetIP[3] = ((uint8_t*)&server_ip)[3];
	
//	targetIP[0] = 10;
//	targetIP[1] = 0;
//	targetIP[2] = 0;
//	targetIP[3] = 22;
}

int application_update(void)
{
		uint32_t tftp_server;
		uint8_t *filename;
		int ret;
 
		tftp_server = (targetIP[0] << 24) | (targetIP[1] << 16) | (targetIP[2] << 8) | (targetIP[3]);
		filename = (uint8_t *)FILE_NAME;

		TFTP_read_request(tftp_server, filename);

		while(1) {
			ret = TFTP_run();
			if(ret != TFTP_PROGRESS)
				break;
		}

		return ret;
}

uint32_t nrf_dfu_init()
{
    uint32_t ret_val = NRF_SUCCESS;
    uint32_t enter_bootloader_mode = 0;
    uint32_t *check_app;

    NRF_LOG_INFO("In real nrf_dfu_init");

    nrf_dfu_settings_init(false);

    // Continue ongoing DFU operations
    // Note that this part does not rely on SoftDevice interaction
    ret_val = nrf_dfu_continue(&enter_bootloader_mode);
    if(ret_val != NRF_SUCCESS)
    {
        NRF_LOG_INFO("Could not continue DFU operation: 0x%08x");
        enter_bootloader_mode = 1;
    }

    // Check if there is a reason to enter DFU mode
    // besides the effect of the continuation
    if (nrf_dfu_enter_check())
    {
        NRF_LOG_INFO("Application sent bootloader request");
        enter_bootloader_mode = 1;
    }
    
    check_app = (uint32_t *)MAIN_APPLICATION_START_ADDR;
    
    if((enter_bootloader_mode != 0) || (*check_app) == 0xFFFFFFFF) //|| !nrf_dfu_app_is_valid())
    {
        //tftp bootloader
        NRF_LOG_INFO("Enter Boot Mode");
        user_ethernet_init();
        TFTP_init(SOCK_TFTP, g_socket_rcv_buf);
				set_target_ip();
        application_update();
    }

    if (nrf_dfu_app_is_valid())
    {
        NRF_LOG_INFO("Jumping to: 0x%08x", MAIN_APPLICATION_START_ADDR);
			
				NRF_LOG_FLUSH();
				nrf_bootloader_app_start(MAIN_APPLICATION_START_ADDR);
    }

    // Should not be reached!
    NRF_LOG_INFO("After real nrf_dfu_init");
    return NRF_SUCCESS;
}
