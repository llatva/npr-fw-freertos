# CLI Implementation Summary

## Overview
Implemented a dual-interface CLI system for the NPR-70 modem firmware with code reuse via shared library.

## Architecture

### Shared CLI Library
**Files:** `Application/Common/cli_commands.h/c`

The CLI library provides:
- Command parsing and processing
- Configuration management
- Status reporting
- System control (reboot, factory reset)

**Key Functions:**
- `CLI_Init()` - Initialize CLI context with output callback
- `CLI_ProcessCommand()` - Parse and execute commands
- `CLI_SendWelcome()` - Send banner message
- `CLI_SendPrompt()` - Send "ready>" prompt

### USB Serial CLI
**Files:** `Application/Tasks/task_serial_cli.h/c`

Features:
- UART-based console on USART2 (PA2/PA15)
- 921600 baud, 8N1
- Character echo, backspace support
- Line editing with Ctrl+C interrupt
- Boots directly to interactive prompt

**Implementation:**
- FreeRTOS task (512 bytes stack, priority 1)
- Blocking UART reads with 100ms timeout
- Output via callback function to CLI library

### Telnet CLI
**Files:** `Application/Tasks/task_telnet.c` (refactored)

Features:
- Network-based console on TCP port 23
- Telnet protocol negotiation
- Session management with timeout
- Identical command set to Serial CLI

**Refactoring:**
- Removed 485 lines of duplicate command processing code
- Now uses shared CLI library
- Output via callback function through W5500

## Command Set
Both interfaces support identical commands:

```
help, ?, apua     - Show command list (apua = Finnish for help)
version           - Firmware version
status            - Detailed modem status (enhanced)
who               - Detailed client information (enhanced)
show config       - Configuration (detailed)
show tasks        - FreeRTOS tasks
show memory       - Heap usage
show dhcp         - DHCP/ARP table
radio on/off      - Radio control
radio diag        - Radio chip diagnostics
test tx <count>   - Send N test packets (1-1000)
save              - Save to flash
set <param> <val> - Set parameter (see below)
reset_to_default  - Factory reset
reboot            - System restart
exit, logout      - Close connection
73                - Ham radio goodbye
```

### Enhanced Status Command
The `status` command provides comprehensive modem status:
- **Basic Info:** Mode, radio state, client ID, connection state, uptime
- **Link Quality:** Timing advance (TA) in units and km, temperature
- **Downlink Metrics:** RSSI (dBm), BER (%) for clients
- **Uplink Metrics:** RSSI (dBm), BER (%) for connected clients
- **Packet Counters:** RX Ethernet, TX Radio, RX Radio

**Implementation:**
- Uses TDMA_table_TA[] for timing advance calculation
- Converts RSSI: (value / 512.0) - 136.0 dBm (downlink), (value / 2.0) - 136.0 dBm (uplink)
- Converts BER: value / 500.0 %
- Distance estimate: TA_units * 0.15 km

### Enhanced Who Command
The `who` command shows detailed client table:

**Master Mode:**
- Own callsign, client ID (127), and IP address
- List of connected clients with:
  - Client index and callsign
  - Assigned client ID
  - IP range allocation (start-end)
  - Connection age in seconds
- Total count of connected clients

**Client Mode:**
- Own callsign, client ID, and IP address
- Connection state (Connected/Waiting/Disconnected)
- Master station callsign

### Radio Diagnostics Command
The `radio diag` command displays:
- Chip type (Si4463)
- Current state (ON/OFF)
- Operating frequency with shift
- RF power level (hex and approximate dBm)
- Network ID
- Chip temperature

### Test Packet Command
The `test tx <count>` command:
- Sends specified number of test packets (1-1000)
- Queues packets to radio TX task
- Provides framework for link testing
- **Note:** Full implementation requires radio task integration for packet generation and result tracking

### Available SET Parameters
The following parameters can be configured via `set <param> <value>`:

**Radio Configuration:**
- `callsign` - Station callsign (up to 13 chars)
- `is_master` - Master/client mode (yes/no)
- `network_id`, `radio_netw_ID` - Network ID (0-15)
- `frequency` - Operating frequency in MHz (420.000-450.000)
- `freq_shift` - Frequency shift in MHz (-10.0 to +10.0)
- `modulation` - Modulation scheme (11-14 or 20-24)
- `RF_power` - RF transmit power (0-127, default 127)
- `radio_on_at_start` - Auto-start radio at boot (yes/no)
- `master_FDD` - Master FDD mode (no/down/up)

**Network Configuration:**
- `modem_IP` - Modem IP address
- `netmask` - Network subnet mask
- `IP_begin` - Starting IP for allocation
- `master_IP_size` - IP pool size (master)
- `client_req_size` - Requested IP size (client)
- `master_down_IP` - FDD master down IP

**Service Configuration:**
- `telnet_active` - Enable telnet service (yes/no)
- `telnet_routed` - Enable telnet routing (yes/no)
- `DHCP_active` - Enable DHCP server (yes/no)
- `DNS_active` - Enable DNS service (yes/no)
- `DNS_value` - DNS server IP address
- `def_route_active` - Enable default route (yes/no)
- `def_route_val` - Default gateway IP address

## Code Savings
- **Eliminated duplication:** ~485 lines of command processing
- **Shared library:** ~430 lines in `cli_commands.c`
- **Net code reduction:** ~55 lines
- **Maintenance benefit:** Single codebase for all commands
- **Flash usage:** +1,040 bytes (56,472 vs 55,432 bytes)
  - Added Serial CLI task (~600 bytes)
  - Added CLI library (~440 bytes)

## Memory Impact
- **Flash:** 56,472 bytes (22.0% of 256KB)
- **RAM:** 59,560 bytes (93% of 64KB)
- **Serial CLI stack:** 512 bytes
- **CLI response buffer:** 400 bytes per interface
- **Heap:** Unchanged at 16KB

## Usage

### Serial Console
```bash
screen /dev/ttyUSB0 921600
```

### Telnet Console
```bash
telnet <modem-ip> 23
```

Both provide identical user experience with full command functionality.

## Implementation Notes

### Output Abstraction
The CLI library uses a function pointer callback:
```c
typedef void (*CLI_OutputFunc_t)(const uint8_t *data, uint16_t len, void *user_data);
```

Each interface provides its own output function:
- **Serial:** `HAL_UART_Transmit()`
- **Telnet:** `W5500_SendData()`

### Command Return Codes
`CLI_ProcessCommand()` returns:
- `0` - Normal command execution
- `1` - Exit/logout requested
- `2` - Reboot requested

This allows each interface to handle these special cases appropriately.

### Thread Safety
- Serial CLI: Single task, no locking needed
- Telnet CLI: Protected by SPI3 mutex for W5500 access
- Shared variables: Protected by existing config mutex

## Future Enhancements
- Command history (up/down arrows)
- Tab completion
- Command aliases
- Multi-line input for complex commands
- Color support for status displays

## Notes on Differences from Original

### Implemented from Original
All major CLI commands from the original MBED implementation have been ported:
- Complete `set` parameter support (25+ parameters)
- Detailed `show config` output matching original format
- Network configuration commands (IP, netmask, DNS, gateway, etc.)
- Radio configuration (frequency, power, modulation, FDD modes)
- System configuration (telnet routing, DHCP, etc.)
- All status and information commands

### Enhanced Features (Beyond Original)
The FreeRTOS port implements enhanced static commands suitable for the task-based architecture:

**1. Enhanced Status Command:**
- Comprehensive link quality metrics (TA, RSSI, BER)
- Temperature monitoring from Si4463
- Packet counter statistics
- Distance estimation from timing advance
- Replaces original's interactive status display with detailed snapshot

**2. Enhanced Who Command:**
- Detailed client table with IP range allocation
- Connection age tracking
- Master and client information in one view
- Replaces original's simpler client list

**3. Radio Diagnostics Command:**
- Si4463 chip status and configuration
- Power level in both hex and dBm
- Frequency and shift information
- Temperature monitoring
- **New feature** not in original

**4. Test Packet Command:**
- Send N test packets for link testing
- Framework for tracking sent/ack/failed counts
- More practical than pure TX_test for production use
- Replaces original's TX_test (carrier test mode)

### Intentionally Not Implemented
- **Original TX_test command**: Hardware carrier test mode requiring direct radio driver access and timing coordination with TDMA. Replaced with more practical test packet command.
- **Interactive displays with continuous updates**: The original had continuously updating status/who displays with Ctrl+C to exit. The FreeRTOS version provides enhanced static snapshots that are better suited to the task-based architecture and provide more information per query.
