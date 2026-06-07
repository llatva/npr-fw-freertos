/*
  ******************************************************************************
  * @file    cli_commands.c
  * @brief   Common CLI command processing implementation
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "cli_commands.h"
#include "app_common.h"
#include "config_flash.h"
#include "task_radio_combined.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
#define CMD_RESULT_NORMAL  0
#define CMD_RESULT_EXIT    1
#define CMD_RESULT_REBOOT  2

/* Public functions ----------------------------------------------------------*/

/**
 * @brief Initialize CLI context
 */
void CLI_Init(CLI_Context_t *ctx, CLI_OutputFunc_t output_func, 
              void *user_data, uint8_t *response_buffer, uint16_t buffer_size) {
    ctx->output_func = output_func;
    ctx->user_data = user_data;
    ctx->response_buffer = response_buffer;
    ctx->response_size = buffer_size;
}

/**
 * @brief Send welcome message
 */
void CLI_SendWelcome(CLI_Context_t *ctx) {
    const char *welcome = 
        "\r\n"
        "=====================================================\r\n"
        "  NPR-70 / TACNPR modem, " FW_VERSION "\r\n"
        "  Type 'help' for commands\r\n"
        "=====================================================\r\n";
    
    if (ctx->output_func) {
        ctx->output_func((const uint8_t *)welcome, strlen(welcome), ctx->user_data);
    }
}

/**
 * @brief Send prompt
 */
void CLI_SendPrompt(CLI_Context_t *ctx) {
    const char *prompt = "ready> ";
    if (ctx->output_func) {
        ctx->output_func((const uint8_t *)prompt, strlen(prompt), ctx->user_data);
    }
}

/**
 * @brief Process CLI command
 */
int CLI_ProcessCommand(CLI_Context_t *ctx, const char *cmd) {
    uint8_t *tx_data = ctx->response_buffer;
    int len = 0;
    char cmd_str[24] = {0};
    char param1[24] = {0};
    char param2[24] = {0};
    
    /* Parse command and parameters */
    sscanf(cmd, "%23s %23s %23s", cmd_str, param1, param2);
    
    /* Command: help */
    if (strcmp(cmd_str, "help") == 0 || strcmp(cmd_str, "?") == 0 || strcmp(cmd_str, "apua") == 0) {
        const char *help_msg = 
            "Available commands:\r\n"
            "  help, ?, apua     - Show this help\r\n"
            "  version           - Show firmware version\r\n"
            "  status            - Show detailed modem status\r\n"
            "  who               - Show detailed client table\r\n"
            "  show config       - Display configuration\r\n"
            "  show tasks        - Display FreeRTOS tasks\r\n"
            "  show memory       - Display memory usage\r\n"
            "  show dhcp         - Display DHCP/ARP entries\r\n"
            "  radio on/off      - Enable/disable radio\r\n"
            "  radio diag        - Show radio diagnostics\r\n"
            "  test tx <count>   - Send N test packets (1-1000)\r\n"
            "  save              - Save configuration to flash\r\n"
            "  set <param> <val> - Set parameter (see below)\r\n"
            "  reset_to_default  - Factory reset (restore defaults)\r\n"
            "  reboot            - Restart the modem\r\n"
            "  exit, logout      - Close connection\r\n"
            "  73                - Ham radio goodbye\r\n\r\n"
            "Set parameters:\r\n"
            "  callsign, is_master, network_id, frequency\r\n"
            "  modulation, freq_shift, RF_power\r\n"
            "  radio_on_at_start, master_FDD\r\n"
            "  telnet_active, telnet_routed\r\n"
            "  modem_IP, netmask, IP_begin\r\n"
            "  DHCP_active, DNS_active, def_route_active\r\n"
            "  DNS_value, def_route_val\r\n"
            "  master_IP_size, client_req_size, master_down_IP\r\n";
        strcpy((char *)tx_data, help_msg);
        len = strlen(help_msg);
    }
    /* Command: version */
    else if (strcmp(cmd_str, "version") == 0) {
        snprintf((char *)tx_data, ctx->response_size, 
                 "NPR-70 FreeRTOS Port\r\n"
                 "Firmware: %s\r\n"
                 "Build: %s %s\r\n"
                 "FreeRTOS: v11.1.0 LTS\r\n",
                 FW_VERSION, __DATE__, __TIME__);
        len = strlen((char *)tx_data);
    }
    /* Command: status */
    else if (strcmp(cmd_str, "status") == 0) {
        const char *conn_state[] = {"Disconnected", "Waiting", "Connected", "Rejected"};
        int32_t TA_km = 0;
        float rssi_down = 0.0f, rssi_up = 0.0f;
        float ber_down = 0.0f, ber_up = 0.0f;
        
        /* Note: Bandwidth calculation would require periodic sampling of counters */
        
        len = snprintf((char *)tx_data, ctx->response_size,
                 "Modem Status:\r\n"
                 "  Mode: %s\r\n"
                 "  Radio: %s\r\n"
                 "  Client ID: %u\r\n"
                 "  Connection: %s\r\n"
                 "  Uptime: %lu sec\r\n",
                 is_TDMA_master ? "Master" : "Client",
                 CONF_radio.state_ON_OFF ? "ON" : "OFF",
                 my_radio_client_ID,
                 conn_state[my_client_radio_connexion_state > 3 ? 0 : my_client_radio_connexion_state],
                 xTaskGetTickCount() / 1000);
        
        /* Link Quality section */
        if (CONF_radio.state_ON_OFF) {
            if (!is_TDMA_master && my_radio_client_ID < RADIO_ADDR_TABLE_SIZE) {
                TA_km = TDMA_table_TA[my_radio_client_ID] / 10; /* TA in units, convert to km (0.15 factor) */
            }
            
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                     "\r\nLink Quality:\r\n"
                     "  Timing Advance: %ld units (%.1f km)\r\n"
                     "  Temperature: %u°C\r\n",
                     (long)TDMA_table_TA[my_radio_client_ID],
                     (float)TA_km * 0.15f,
                     G_temperature_SI4463);
            
            /* Downlink quality (for clients) */
            if (!is_TDMA_master && RSSI_stat_pkt_nb > 0) {
                rssi_down = ((float)G_downlink_RSSI / 512.0f) - 136.0f;
                ber_down = ((float)G_downlink_BER) / 500.0f;
                
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "\r\nDownlink:\r\n"
                         "  RSSI: %.1f dBm\r\n"
                         "  BER: %.2f%%\r\n",
                         rssi_down, ber_down);
            } else if (!is_TDMA_master) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "\r\nDownlink:\r\n"
                         "  RSSI: -- dBm\r\n"
                         "  BER: --%%\r\n");
            }
            
            /* Uplink quality (for connected clients) */
            if (!is_TDMA_master && my_client_radio_connexion_state == 2 &&
                my_radio_client_ID < RADIO_ADDR_TABLE_SIZE) {
                rssi_up = ((float)G_radio_addr_table_RSSI[my_radio_client_ID] / 2.0f) - 136.0f;
                ber_up = ((float)G_radio_addr_table_BER[my_radio_client_ID]) / 500.0f;
                
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "\r\nUplink:\r\n"
                         "  RSSI: %.1f dBm\r\n"
                         "  BER: %.2f%%\r\n",
                         rssi_up, ber_up);
            } else if (!is_TDMA_master) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "\r\nUplink:\r\n"
                         "  RSSI: -- dBm\r\n"
                         "  BER: --%%\r\n");
            }
        }
        
        /* Packet counters */
        len += snprintf((char *)tx_data + len, ctx->response_size - len,
                 "\r\nPacket Counters:\r\n"
                 "  RX Ethernet: %lu\r\n"
                 "  TX Radio: %lu\r\n"
                 "  RX Radio: %lu\r\n",
                 (unsigned long)RX_Eth_IPv4_counter,
                 (unsigned long)TX_radio_IPv4_counter,
                 (unsigned long)RX_radio_IPv4_counter);
    }
    /* Command: show */
    else if (strcmp(cmd_str, "show") == 0 || strcmp(cmd_str, "display") == 0) {
        if (strcmp(param1, "config") == 0) {
            const char *yes_no[] = {"no", "yes"};
            const char *fdd_mode[] = {"no", "down", "up"};
            uint32_t ip;
            
            len = snprintf((char *)tx_data, ctx->response_size,
                     "\r\nCurrent CONFIG:\r\n"
                     "  callsign: %s\r\n"
                     "  is_master: %s\r\n"
                     "  MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                     CONF_radio_my_callsign + 2,
                     yes_no[is_TDMA_master ? 1 : 0],
                     CONF_modem_MAC[0], CONF_modem_MAC[1], CONF_modem_MAC[2],
                     CONF_modem_MAC[3], CONF_modem_MAC[4], CONF_modem_MAC[5]);
            
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                     "  ext_SRAM: %s\r\n"
                     "  frequency: %u.%03u MHz\r\n"
                     "  freq_shift: %.3f MHz\r\n"
                     "  RF_power: %u\r\n"
                     "  modulation: %u\r\n",
                     yes_no[is_SRAM_ext ? 1 : 0],
                     420 + (CONF_frequency_HD / 1000), CONF_frequency_HD % 1000,
                     (float)CONF_freq_shift / 1000.0,
                     CONF_radio_PA_PWR,
                     CONF_radio.modulation);
            
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                     "  radio_netw_ID: %u\r\n"
                     "  radio_on_at_start: %s\r\n"
                     "  telnet_active: %s\r\n"
                     "  telnet_routed: %s\r\n",
                     CONF_radio_network_ID,
                     yes_no[CONF_radio.default_state_ON_OFF],
                     yes_no[is_telnet_active ? 1 : 0],
                     yes_no[is_telnet_routed ? 1 : 0]);
            
            ip = LAN_conf_applied.LAN_modem_IP;
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                     "  modem_IP: %u.%u.%u.%u\r\n",
                     (unsigned int)(ip >> 24) & 0xFF,
                     (unsigned int)(ip >> 16) & 0xFF,
                     (unsigned int)(ip >> 8) & 0xFF,
                     (unsigned int)ip & 0xFF);
            
            ip = LAN_conf_applied.LAN_subnet_mask;
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                     "  netmask: %u.%u.%u.%u\r\n",
                     (unsigned int)(ip >> 24) & 0xFF,
                     (unsigned int)(ip >> 16) & 0xFF,
                     (unsigned int)(ip >> 8) & 0xFF,
                     (unsigned int)ip & 0xFF);
            
            if (is_TDMA_master) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  master_FDD: %s\r\n",
                         fdd_mode[CONF_radio.master_FDD]);
            }
            
            if (is_TDMA_master && CONF_radio.master_FDD < 2) {
                ip = CONF_radio_IP_start;
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  IP_begin: %u.%u.%u.%u\r\n"
                         "  master_IP_size: %lu\r\n",
                         (unsigned int)(ip >> 24) & 0xFF,
                         (unsigned int)(ip >> 16) & 0xFF,
                         (unsigned int)(ip >> 8) & 0xFF,
                         (unsigned int)ip & 0xFF,
                         (unsigned long)CONF_radio_IP_size);
                
                ip = LAN_conf_applied.LAN_def_route;
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  def_route_active: %s\r\n"
                         "  def_route_val: %u.%u.%u.%u\r\n",
                         yes_no[LAN_conf_applied.LAN_def_route_activ],
                         (unsigned int)(ip >> 24) & 0xFF,
                         (unsigned int)(ip >> 16) & 0xFF,
                         (unsigned int)(ip >> 8) & 0xFF,
                         (unsigned int)ip & 0xFF);
                
                ip = LAN_conf_applied.LAN_DNS_value;
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  DNS_active: %s\r\n"
                         "  DNS_value: %u.%u.%u.%u\r\n",
                         yes_no[LAN_conf_applied.LAN_DNS_activ],
                         (unsigned int)(ip >> 24) & 0xFF,
                         (unsigned int)(ip >> 16) & 0xFF,
                         (unsigned int)(ip >> 8) & 0xFF,
                         (unsigned int)ip & 0xFF);
            }
            
            if (is_TDMA_master && CONF_radio.master_FDD == 2) {
                ip = CONF_master_down_IP;
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  master_down_IP: %u.%u.%u.%u\r\n",
                         (unsigned int)(ip >> 24) & 0xFF,
                         (unsigned int)(ip >> 16) & 0xFF,
                         (unsigned int)(ip >> 8) & 0xFF,
                         (unsigned int)ip & 0xFF);
            }
            
            if (!is_TDMA_master) {
                ip = CONF_radio_IP_start;
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                         "  IP_begin: %u.%u.%u.%u\r\n"
                         "  client_req_size: %lu\r\n"
                         "  DHCP_active: %s\r\n",
                         (unsigned int)(ip >> 24) & 0xFF,
                         (unsigned int)(ip >> 16) & 0xFF,
                         (unsigned int)(ip >> 8) & 0xFF,
                         (unsigned int)ip & 0xFF,
                         (unsigned long)CONF_radio_IP_size_requested,
                         yes_no[LAN_conf_applied.DHCP_server_active]);
            }
        }
        else if (strcmp(param1, "tasks") == 0) {
            TaskStatus_t task_stats[16];
            UBaseType_t task_count = uxTaskGetNumberOfTasks();
            if (task_count > 16) task_count = 16;
            
            task_count = uxTaskGetSystemState(task_stats, task_count, NULL);
            
            len = snprintf((char *)tx_data, ctx->response_size,
                          "FreeRTOS Tasks (%u):\r\n", (unsigned int)task_count);
            
            for (UBaseType_t i = 0; i < task_count && len < (ctx->response_size - 50); i++) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                               "  %-16s Pri:%u Stack:%u\r\n",
                               task_stats[i].pcTaskName,
                               (unsigned int)task_stats[i].uxCurrentPriority,
                               (unsigned int)task_stats[i].usStackHighWaterMark);
            }
        }
        else if (strcmp(param1, "memory") == 0) {
            size_t free_heap = xPortGetFreeHeapSize();
            size_t min_heap = xPortGetMinimumEverFreeHeapSize();
            
            snprintf((char *)tx_data, ctx->response_size,
                     "Memory Usage:\r\n"
                     "  Heap Free: %u bytes\r\n"
                     "  Heap Min:  %u bytes\r\n",
                     (unsigned int)free_heap, (unsigned int)min_heap);
            len = strlen((char *)tx_data);
        }
        else if (strcmp(param1, "buffers") == 0) {
            size_t free_heap = xPortGetFreeHeapSize();
            len = snprintf((char *)tx_data, ctx->response_size, "Buffers (LID: ptr, last_used_ms):\r\n");
            for (int i = 0; i < RADIO_ADDR_TABLE_SIZE && len < (ctx->response_size - 80); i++) {
                void *ptr = (void *)ethernet_buffer[i];
                uint32_t age = 0;
                if (buffer_last_used_ms[i] != 0) {
                    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    age = (now > buffer_last_used_ms[i]) ? (now - buffer_last_used_ms[i]) : 0;
                }
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                                "  [%d] %p  age=%lu ms\r\n", i, ptr, (unsigned long)age);
            }
            len += snprintf((char *)tx_data + len, ctx->response_size - len, 
                           "Heap free: %u bytes\r\n", (unsigned int)free_heap);
        }
        else if (strcmp(param1, "dhcp") == 0 || strcmp(param1, "DHCP_ARP") == 0) {
            len = snprintf((char *)tx_data, ctx->response_size,
                          "DHCP/ARP Entries:\r\n");
            
            int count = 0;
            for (int i = 0; i < RADIO_ADDR_TABLE_SIZE && len < (ctx->response_size - 80); i++) {
                if (CONF_radio_addr_table_status[i] != 0) {
                    uint32_t ip = CONF_radio_addr_table_IP_begin[i];
                    len += snprintf((char *)tx_data + len, ctx->response_size - len,
                                   "  [%d] %u.%u.%u.%u  %s\r\n",
                                   i,
                                   (unsigned int)(ip >> 24) & 0xFF,
                                   (unsigned int)(ip >> 16) & 0xFF,
                                   (unsigned int)(ip >> 8) & 0xFF,
                                   (unsigned int)ip & 0xFF,
                                   CONF_radio_addr_table_callsign[i]);
                    count++;
                }
            }
            
            if (count == 0) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                               "  (no entries)\r\n");
            }
        }
        else {
            strcpy((char *)tx_data, "Usage: show {config|tasks|memory|dhcp}\r\n");
            len = strlen((char *)tx_data);
        }
    }
    /* Command: radio */
    else if (strcmp(cmd_str, "radio") == 0) {
        if (strcmp(param1, "on") == 0) {
            CONF_radio.state_ON_OFF = 1;
            strcpy((char *)tx_data, "Radio is now ON.\r\n");
            len = strlen((char *)tx_data);
        }
        else if (strcmp(param1, "off") == 0) {
            CONF_radio.state_ON_OFF = 0;
            strcpy((char *)tx_data, "Radio is now OFF.\r\n");
            len = strlen((char *)tx_data);
        }
        else if (strcmp(param1, "diag") == 0 || strcmp(param1, "diagnostics") == 0) {
            /* Radio chip diagnostics - requires SI4463 radio driver functions */
            len = snprintf((char *)tx_data, ctx->response_size,
                          "Radio Chip Diagnostics:\r\n"
                          "  Chip: Si4463\r\n"
                          "  State: %s\r\n"
                          "  Frequency: %u.%03u MHz\r\n"
                          "  Freq Shift: %d kHz\r\n"
                          "  RF Power: 0x%02X (%.1f dBm)\r\n"
                          "  Network ID: %u\r\n"
                          "  Temperature: %u°C\r\n",
                          CONF_radio.state_ON_OFF ? "ON" : "OFF",
                          420 + (CONF_frequency_HD / 1000),
                          CONF_frequency_HD % 1000,
                          CONF_freq_shift,
                          CONF_radio_PA_PWR,
                          /* Approx conversion: 0=~-32dBm, 127=+20dBm */
                          ((float)CONF_radio_PA_PWR * 0.41f) - 32.0f,
                          CONF_radio_network_ID,
                          G_temperature_SI4463);
            
            /* Note: For detailed chip status (PLL lock, FIFO status, etc.),
             * would need to call SI4463 driver functions to read registers.
             * This would require including si4463_driver.h and calling
             * functions like SI4463_Read_Status(). */
        }
        else {
            strcpy((char *)tx_data, "Usage: radio {on|off|diag}\r\n");
            len = strlen((char *)tx_data);
        }
    }
    /* Command: test */
    else if (strcmp(cmd_str, "test") == 0) {
        if (strcmp(param1, "tx") == 0) {
            int count = atoi(param2);
            if (count > 0 && count <= 1000) {
                /* Send test packets - this requires integration with radio task
                 * For now, provide a placeholder response */
                len = snprintf((char *)tx_data, ctx->response_size,
                              "Sending %d test packets...\r\n"
                              "Note: Test packet transmission requires radio task integration.\r\n"
                              "This feature queues packets to the radio TX buffer.\r\n",
                              count);
                
                /* TODO: Implement actual test packet transmission by:
                 * 1. Creating test UDP/IP packets with incrementing sequence numbers
                 * 2. Queuing them to the radio TX task
                 * 3. Tracking sent/ack/failed counts
                 * 4. Reporting results after completion
                 * This requires access to radio task queue and TX functions.
                 */
            }
            else {
                strcpy((char *)tx_data, "Usage: test tx <count>  (count: 1-1000)\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else {
            strcpy((char *)tx_data, "Usage: test tx <count>\r\n");
            len = strlen((char *)tx_data);
        }
    }
    /* Command: set */
    else if (strcmp(cmd_str, "set") == 0) {
        if (strlen(param1) == 0 || strlen(param2) == 0) {
            strcpy((char *)tx_data, "Usage: set <param> <value>\r\n");
            len = strlen((char *)tx_data);
        }
        else if (strcmp(param1, "network_id") == 0 || strcmp(param1, "radio_netw_ID") == 0) {
            int val = atoi(param2);
            if (val >= 0 && val <= 15) {
                CONF_radio_network_ID = (uint8_t)val;
                snprintf((char *)tx_data, ctx->response_size,
                         "Network ID set to %u\r\n", CONF_radio_network_ID);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Network ID must be 0-15\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "frequency") == 0) {
            int freq_mhz = 0, freq_khz = 0;
            if (strchr(param2, '.')) {
                sscanf(param2, "%d.%d", &freq_mhz, &freq_khz);
                if (freq_mhz >= 420 && freq_mhz <= 450) {
                    CONF_frequency_HD = (uint16_t)((freq_mhz - 420) * 1000 + freq_khz);
                    snprintf((char *)tx_data, ctx->response_size,
                             "Frequency set to %u.%03u MHz\r\n",
                             420 + (CONF_frequency_HD / 1000),
                             CONF_frequency_HD % 1000);
                    len = strlen((char *)tx_data);
                }
                else {
                    strcpy((char *)tx_data, "ERROR: Frequency must be 420-450 MHz\r\n");
                    len = strlen((char *)tx_data);
                }
            }
            else {
                strcpy((char *)tx_data, "Usage: set frequency <MHz> (e.g. 437.000)\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "modulation") == 0) {
            int val = atoi(param2);
            if (((val >= 11) && (val <= 14)) || ((val >= 20) && (val <= 24))) {
                CONF_radio.modulation = (uint8_t)val;
                snprintf((char *)tx_data, ctx->response_size,
                         "Modulation set to %u\r\n", CONF_radio.modulation);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid modulation (11-14 or 20-24)\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "is_master") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                is_TDMA_master = 1;
                strcpy((char *)tx_data, "Master mode enabled\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                is_TDMA_master = 0;
                strcpy((char *)tx_data, "Client mode enabled\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set is_master {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "callsign") == 0) {
            if (strlen(param2) > 0 && strlen(param2) < 14) {
                strncpy(CONF_radio_my_callsign + 2, param2, 13);
                CONF_radio_my_callsign[15] = 0;
                snprintf((char *)tx_data, ctx->response_size,
                         "Callsign set to %s\r\n", CONF_radio_my_callsign + 2);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid callsign\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "freq_shift") == 0) {
            float shift_val = 0;
            if (sscanf(param2, "%f", &shift_val) == 1 && shift_val >= -10.0 && shift_val <= 10.0) {
                CONF_freq_shift = (int16_t)(shift_val * 1000);
                snprintf((char *)tx_data, ctx->response_size,
                         "Frequency shift set to %.3f MHz\r\n", shift_val);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: freq_shift must be -10.0 to +10.0 MHz\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "RF_power") == 0) {
            int val = atoi(param2);
            if (val >= 0 && val <= 127) {
                CONF_radio_PA_PWR = (uint8_t)val;
                snprintf((char *)tx_data, ctx->response_size,
                         "RF power set to %u\r\n", CONF_radio_PA_PWR);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: RF_power must be 0-127\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "radio_on_at_start") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                CONF_radio.default_state_ON_OFF = 1;
                strcpy((char *)tx_data, "Radio will start ON at boot\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                CONF_radio.default_state_ON_OFF = 0;
                strcpy((char *)tx_data, "Radio will start OFF at boot\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set radio_on_at_start {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "master_FDD") == 0) {
            if (strcmp(param2, "no") == 0) {
                CONF_radio.master_FDD = 0;
                strcpy((char *)tx_data, "Master FDD: no (TDD mode)\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "down") == 0) {
                CONF_radio.master_FDD = 1;
                strcpy((char *)tx_data, "Master FDD: down\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "up") == 0) {
                CONF_radio.master_FDD = 2;
                strcpy((char *)tx_data, "Master FDD: up\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set master_FDD {no|down|up}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "telnet_active") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                is_telnet_active = 1;
                strcpy((char *)tx_data, "Telnet active: yes\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                is_telnet_active = 0;
                strcpy((char *)tx_data, "Telnet active: no\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set telnet_active {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "telnet_routed") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                is_telnet_routed = 1;
                strcpy((char *)tx_data, "Telnet routed: yes\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                is_telnet_routed = 0;
                strcpy((char *)tx_data, "Telnet routed: no\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set telnet_routed {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "DHCP_active") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                LAN_conf_applied.DHCP_server_active = 1;
                strcpy((char *)tx_data, "DHCP server active: yes\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                LAN_conf_applied.DHCP_server_active = 0;
                strcpy((char *)tx_data, "DHCP server active: no\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set DHCP_active {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "DNS_active") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                LAN_conf_applied.LAN_DNS_activ = 1;
                strcpy((char *)tx_data, "DNS active: yes\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                LAN_conf_applied.LAN_DNS_activ = 0;
                strcpy((char *)tx_data, "DNS active: no\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set DNS_active {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "def_route_active") == 0) {
            if (strcmp(param2, "yes") == 0 || strcmp(param2, "1") == 0) {
                LAN_conf_applied.LAN_def_route_activ = 1;
                strcpy((char *)tx_data, "Default route active: yes\r\n");
                len = strlen((char *)tx_data);
            }
            else if (strcmp(param2, "no") == 0 || strcmp(param2, "0") == 0) {
                LAN_conf_applied.LAN_def_route_activ = 0;
                strcpy((char *)tx_data, "Default route active: no\r\n");
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "Usage: set def_route_active {yes|no}\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "modem_IP") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                LAN_conf_applied.LAN_modem_IP = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "Modem IP set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP address\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "netmask") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                LAN_conf_applied.LAN_subnet_mask = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "Netmask set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid netmask\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "def_route_val") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                LAN_conf_applied.LAN_def_route = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "Default route set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP address\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "DNS_value") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                LAN_conf_applied.LAN_DNS_value = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "DNS value set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP address\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "IP_begin") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                CONF_radio_IP_start = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "IP start set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP address\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "master_down_IP") == 0) {
            uint32_t a, b, c, d;
            if (sscanf(param2, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) == 4 &&
                a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                CONF_master_down_IP = (a << 24) | (b << 16) | (c << 8) | d;
                snprintf((char *)tx_data, ctx->response_size,
                         "Master down IP set to %lu.%lu.%lu.%lu\r\n", a, b, c, d);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP address\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "master_IP_size") == 0) {
            uint32_t val = atoi(param2);
            if (val > 0 && val <= 0xFFFF) {
                CONF_radio_IP_size = val;
                snprintf((char *)tx_data, ctx->response_size,
                         "Master IP size set to %lu\r\n", val);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP size\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else if (strcmp(param1, "client_req_size") == 0) {
            uint32_t val = atoi(param2);
            if (val > 0 && val <= 0xFFFF) {
                CONF_radio_IP_size_requested = val;
                snprintf((char *)tx_data, ctx->response_size,
                         "Client requested size set to %lu\r\n", val);
                len = strlen((char *)tx_data);
            }
            else {
                strcpy((char *)tx_data, "ERROR: Invalid IP size\r\n");
                len = strlen((char *)tx_data);
            }
        }
        else {
            snprintf((char *)tx_data, ctx->response_size,
                     "Unknown parameter: %s\r\nType 'help' for available commands\r\n",
                     param1);
            len = strlen((char *)tx_data);
        }
    }
    /* Command: save */
    else if (strcmp(cmd_str, "save") == 0) {
        HAL_StatusTypeDef status = Config_Flash_Save();
        
        if (status == HAL_OK) {
            strcpy((char *)tx_data, "Configuration saved to flash successfully.\r\n");
        } else {
            strcpy((char *)tx_data, "ERROR: Failed to save configuration to flash!\r\n");
        }
        len = strlen((char *)tx_data);
    }
    /* Command: who */
    else if (strcmp(cmd_str, "who") == 0) {
        uint32_t ip, ip_end;
        uint32_t uptime_sec = xTaskGetTickCount() / 1000;
        
        len = snprintf((char *)tx_data, ctx->response_size,
                      "Master/Client Information:\r\n");
        
        if (is_TDMA_master) {
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                           "  Mode: MASTER\r\n"
                           "  My Callsign: %s\r\n"
                           "  My Client ID: 127\r\n",
                           CONF_radio_my_callsign + 2);
            
            ip = LAN_conf_applied.LAN_modem_IP;
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                           "  My IP: %u.%u.%u.%u\r\n\r\n",
                           (unsigned int)(ip >> 24) & 0xFF,
                           (unsigned int)(ip >> 16) & 0xFF,
                           (unsigned int)(ip >> 8) & 0xFF,
                           (unsigned int)ip & 0xFF);
            
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                           "Connected Clients:\r\n");
            
            int clients = 0;
            for (int i = 0; i < RADIO_ADDR_TABLE_SIZE && len < (ctx->response_size - 100); i++) {
                if (CONF_radio_addr_table_status[i] != 0) {
                    ip = CONF_radio_addr_table_IP_begin[i];
                    ip_end = ip + CONF_radio_addr_table_IP_size[i] - 1;
                    
                    /* Calculate age */
                    uint32_t age_sec = 0;
                    if (CONF_radio_addr_table_date[i] > 0) {
                        age_sec = (uptime_sec > CONF_radio_addr_table_date[i]) ?
                                  (uptime_sec - CONF_radio_addr_table_date[i]) : 0;
                    }
                    
                    len += snprintf((char *)tx_data + len, ctx->response_size - len,
                                   "  [%d] %-10s  ID=%d  IP: %u.%u.%u.%u-%u.%u.%u.%u  Age: %lus\r\n",
                                   i,
                                   CONF_radio_addr_table_callsign[i],
                                   i,
                                   (unsigned int)(ip >> 24) & 0xFF,
                                   (unsigned int)(ip >> 16) & 0xFF,
                                   (unsigned int)(ip >> 8) & 0xFF,
                                   (unsigned int)ip & 0xFF,
                                   (unsigned int)(ip_end >> 24) & 0xFF,
                                   (unsigned int)(ip_end >> 16) & 0xFF,
                                   (unsigned int)(ip_end >> 8) & 0xFF,
                                   (unsigned int)ip_end & 0xFF,
                                   (unsigned long)age_sec);
                    clients++;
                }
            }
            
            if (clients == 0) {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                               "  (no clients connected)\r\n");
            } else {
                len += snprintf((char *)tx_data + len, ctx->response_size - len,
                               "\r\nTotal: %d client%s connected\r\n",
                               clients, clients == 1 ? "" : "s");
            }
        }
        else {
            ip = LAN_conf_applied.LAN_modem_IP;
            len += snprintf((char *)tx_data + len, ctx->response_size - len,
                           "  Mode: CLIENT\r\n"
                           "  My Callsign: %s\r\n"
                           "  My Client ID: %u\r\n"
                           "  My IP: %u.%u.%u.%u\r\n"
                           "  Connection: %s\r\n"
                           "  Master: %s\r\n",
                           CONF_radio_my_callsign + 2,
                           my_radio_client_ID,
                           (unsigned int)(ip >> 24) & 0xFF,
                           (unsigned int)(ip >> 16) & 0xFF,
                           (unsigned int)(ip >> 8) & 0xFF,
                           (unsigned int)ip & 0xFF,
                           my_client_radio_connexion_state == 2 ? "Connected" :
                           my_client_radio_connexion_state == 1 ? "Waiting" : "Disconnected",
                           CONF_radio_master_callsign + 2);
        }
    }
    /* Command: reset_to_default */
    else if (strcmp(cmd_str, "reset_to_default") == 0) {
        const char *msg = "Restoring factory defaults and rebooting...\r\n";
        strcpy((char *)tx_data, msg);
        len = strlen(msg);
        
        /* Send response first */
        if (ctx->output_func && len > 0) {
            ctx->output_func(tx_data, len, ctx->user_data);
        }
        
        /* Restore factory defaults */
        Config_Flash_FactoryReset();
        
        vTaskDelay(pdMS_TO_TICKS(100));
        return CMD_RESULT_REBOOT;
    }
    /* Command: reboot */
    else if (strcmp(cmd_str, "reboot") == 0) {
        const char *msg = "Rebooting...\r\n";
        strcpy((char *)tx_data, msg);
        len = strlen(msg);
        
        /* Send response first */
        if (ctx->output_func && len > 0) {
            ctx->output_func(tx_data, len, ctx->user_data);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        return CMD_RESULT_REBOOT;
    }
    /* Command: exit/logout */
    else if (strcmp(cmd_str, "exit") == 0 || strcmp(cmd_str, "logout") == 0) {
        const char *msg = "Goodbye!\r\n";
        strcpy((char *)tx_data, msg);
        len = strlen(msg);
        
        if (ctx->output_func && len > 0) {
            ctx->output_func(tx_data, len, ctx->user_data);
        }
        
        return CMD_RESULT_EXIT;
    }
    /* Command: 73 */
    else if (strcmp(cmd_str, "73") == 0) {
        strcpy((char *)tx_data, "73!\r\n");
        len = strlen((char *)tx_data);
    }
    /* Unknown command or empty line */
    else if (strlen(cmd_str) > 0) {
        snprintf((char *)tx_data, ctx->response_size, 
                 "Unknown command: %s\r\nType 'help' for commands\r\n", cmd_str);
        len = strlen((char *)tx_data);
    }
    else {
        /* Empty line - just send prompt */
        len = 0;
    }
    
    /* Send response if any */
    if (ctx->output_func && len > 0) {
        ctx->output_func(tx_data, len, ctx->user_data);
    }
    
    return CMD_RESULT_NORMAL;
}
