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
status            - Modem status
who               - Client information
show config       - Configuration (detailed)
show tasks        - FreeRTOS tasks
show memory       - Heap usage
show dhcp         - DHCP/ARP table
radio on/off      - Radio control
save              - Save to flash
set <param> <val> - Set parameter (see below)
reset_to_default  - Factory reset
reboot            - System restart
exit, logout      - Close connection
73                - Ham radio goodbye
```

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

### Intentionally Not Implemented
- **TX_test command**: Hardware-specific test mode requiring direct radio driver access and timing coordination with TDMA. This is a debug feature not essential for normal operation.
- **Interactive status/who displays**: The original had continuously updating status displays with Ctrl+C to exit. The FreeRTOS version provides static snapshots to avoid CLI complexity.
