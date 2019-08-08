#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "timer_drift_measurement.h"
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "nrf_gpio.h"
#include "time_sync_timer.h"
#include "ethernet.h"
#include "socket.h"
#include "w5500.h"
#include "sync_timer_handler.h"
#include "toolchain.h"
#include "timer.h"


/* Variables for calculating drift */
static uint32_t m_time_tic;

/*Should triggers each time the sync-line is set high by the master node*/
void sync_line_event_handler(void)
{
    m_time_tic++;

    uint32_t was_masked;
    _DISABLE_IRQS(was_masked);

    DRIFT_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t processing_delay = DRIFT_TIMER->CC[0];
    uint32_t now = timer_now() - processing_delay;
    uint32_t adjusted_sync_timer = now - sync_timer_get_current_offset();

    _ENABLE_IRQS(was_masked);

    send_drift_timing_sample(adjusted_sync_timer);
}

/* Function to send timing samples over Ethernet, using UDP */
void send_drift_timing_sample(uint32_t adjusted_sync_timer)
{
        uint8_t buf[SCAN_REPORT_LENGTH];
        uint8_t len = 0;
        uint8_t own_MAC[6] = {0};
        get_own_MAC(own_MAC);


        #ifdef BROADCAST_ENABLED
            uint8_t target_IP[4] = {255, 255, 255, 255}; 
            uint32_t target_port = 11001;;
        #else
            uint8_t target_IP[4];
            get_target_IP(target_IP);       
            uint32_t target_port = 11001;
        #endif

        sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"drift\" : %d, \"timetic\" : %d}", 
                        own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5],
                        adjusted_sync_timer,
                        m_time_tic);
    
        len = strlen((const char *)&buf[0]);
        int32_t err = sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);

        if(err < 0)
        {
          __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Error sending packet (send_drift_timing_sample): %d\n", err);
        }
}

void reset_drift_measure_params(void)
{
m_time_tic = 0;
}