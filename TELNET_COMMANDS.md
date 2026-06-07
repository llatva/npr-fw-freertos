# NPR-70 FreeRTOS Telnet Command Reference

## Connection
- **Port**: TCP 23
- **Protocol**: Telnet with echo and line editing
- **Timeout**: 5 minutes of inactivity
- **Prompt**: `ready>`

## Command Summary

### Information Commands

#### `help` or `?` or `apua`
Display complete command reference.

**Note:** `apua` is Finnish for "help"

#### `version`
Show firmware version information.
```
NPR-70 FreeRTOS Port
Firmware: 2025_11_07-freertos-v1.0
Build: Nov  7 2025 12:34:56
FreeRTOS: v11.1.0 LTS
```

#### `status`
Display comprehensive modem status with link quality and packet counters.
```
Modem Status:
  Mode: Master/Client
  Radio: ON/OFF
  Client ID: <0-15>
  Connection: Connected/Waiting/Disconnected/Rejected
  Uptime: <seconds> sec

Link Quality:
  Timing Advance: <units> (<km>)
  Temperature: <degrees>°C

Downlink:
  RSSI: <value> dBm
  BER: <percentage>%

Uplink:
  RSSI: <value> dBm
  BER: <percentage>%

Packet Counters:
  RX Ethernet: <count>
  TX Radio: <count>
  RX Radio: <count>
```

**Notes:**
- Link quality section only shown when radio is ON
- Downlink/uplink metrics shown for clients only
- Timing advance converted to approximate distance (0.15 km/unit)
- BER (Bit Error Rate) shown as percentage

#### `who`
Show detailed master/client connection information.

**Master mode:**
```
Master/Client Information:
  Mode: MASTER
  My Callsign: MYCALL
  My Client ID: 127
  My IP: 192.168.10.1

Connected Clients:
  [0] CLIENT1     ID=0  IP: 192.168.10.100-192.168.10.199  Age: 45s
  [1] CLIENT2     ID=1  IP: 192.168.10.200-192.168.10.299  Age: 120s

Total: 2 clients connected
```

**Client mode:**
```
Master/Client Information:
  Mode: CLIENT
  My Callsign: MYCLIENT
  My Client ID: 5
  My IP: 192.168.10.205
  Connection: Connected/Waiting/Disconnected
  Master: MASTER_CALL
```

**Notes:**
- Master mode shows IP range allocated to each client
- Age indicates time since client registration (in seconds)
- Client ID 127 is reserved for master

### Display Commands
omplete configuration including network parameters.
```
Current CONFIG:
  callsign: MYCALL
  is_master: no
  MAC: 4E:46:50:52:00:01
  ext_SRAM: yes
  frequency: 437.000 MHz
  freq_shift: 0.000 MHz
  RF_power: 127
  modulation: 20
  radio_netw_ID: 0
  radio_on_at_start: no
  telnet_active: no
  telnet_routed: no
  modem_IP: 192.168.10.1
  netmask: 255.255.255.0
  [additional parameters based on master/client mode]
  Modulation: 22
  Radio State: ON/OFF
```

#### `show tasks`
Display FreeRTOS task information.
```
FreeRTOS Tasks (10):
  RadioISR        Pri:5 Stack:128
  RadioProc       Pri:4 Stack:256
  TDMA            Pri:3 Stack:384
  ...
```

#### `show memory`
Display memory usage statistics.
```
Memory Usage:
  Heap Free: 8192 bytes
  Heap Min:  7456 bytes
  RAM Used:  64648 / 65536 (98.9%)
```

#### `show dhcp` or `display DHCP_ARP`
Display DHCP/ARP table entries.
```
DHCP/ARP Entries:
  [0] 192.168.1.10  STATION1
  [1] 192.168.1.11  STATION2
  [2] 192.168.1.12  STATION3
```

### Control Commands

#### `radio on`
Enable radio transmitter.

#### `radio off`
Disable radio transmitter.

#### `radio diag`
Display detailed radio chip diagnostics.
```
Radio Chip Diagnostics:
  Chip: Si4463
  State: ON/OFF
  Frequency: 437.000 MHz
  Freq Shift: 0 kHz
  RF Power: 0x7F (20.0 dBm)
  Network ID: 0
  Temperature: 42°C
```

**Notes:**
- Power level ranges from 0x00 (-32 dBm) to 0x7F (+20 dBm) approximately
- Temperature read from Si4463 internal sensor
- Additional chip status (PLL lock, FIFO) may be added in future

#### `test tx <count>`
Send test packets over radio link.
```
test tx 100
Sending 100 test packets...
Note: Test packet transmission requires radio task integration.
This feature queues packets to the radio TX buffer.
```

**Parameters:**
- `<count>`: Number of test packets to send (1-1000)

**Notes:**
- Test packets are UDP/IP frames with sequence numbers
- Useful for testing link quality and throughput
- Results will show sent/ack/failed counts when fully implemented

### Configuration Commands

#### `set network_id <0-15>` or `set radio_netw_ID <0-15>`
Set radio network ID (0-15).
```
set network_id 5
Network ID set to 5
```

#### `set frequency <MHz>`
Set operating frequency (420-450 MHz).
```
set frequency 437.000
Frequency set to 437.000 MHz
```

#### `set modulation <11-14|20-24>`
Set modulation scheme.

Valid values:
- 11-14: Lower data rates
- 20-24: Higher data rates

```
set modulation 22
Modulation set to 22
```

#### `set is_master <yes|no>`
Set master/client operating mode.
```
set is_master yes
Master mode enabled
```

#### `set callsign <CALLSIGN>`
Set station callsign (max 13 characters).
```
set callsign OH1CALL
Callsign set to OH1CALL
```

### System Commands

#### `save`
Save configuration to non-volatile memory.
```
Configuration save not yet implemented.
```

#### `reboot`
Restart the modem.
```
Rebooting...
```

#### `reset_to_default`
Factory reset - clear all configuration and reboot.
```
Clearing config and rebooting...
```

#### `exit` or `logout`
Close telnet connection.
```
Goodbye!
```

#### `73`
Ham radio goodbye (Easter egg).
```
73!
```

## Command Parameters

### Available `set` parameters:
- `callsign` - Station callsign (up to 13 chars)
- `is_master` - Master mode (yes/no or 1/0)
- `network_id` or `radio_netw_ID` - Radio network ID (0-15)
- `frequency` - Operating frequency in MHz (420.000-450.000)
- `freq_shift` - Frequency shift in MHz (-10.0 to +10.0)
- `modulation` - Modulation scheme (11-14 or 20-24)
- `RF_power` - RF power (0-127, default 127)
- `radio_on_at_start` - Radio auto-start (yes/no)
- `master_FDD` - Master FDD mode (no/down/up)
- `telnet_active` - Telnet service (yes/no)
- `telnet_routed` - Telnet routing (yes/no)
- `modem_IP` - Modem IP address (e.g., 192.168.10.1)
- `netmask` - Network mask (e.g., 255.255.255.0)
- `IP_begin` - Starting IP for allocation
- `DHCP_active` - DHCP server (yes/no)
- `DNS_active` - DNS service (yes/no)
- `DNS_value` - DNS server IP address
- `def_route_active` - Default route (yes/no)
- `def_route_val` - Default gateway IP
- `master_IP_size` - Master IP pool size
- `client_req_size` - Client requested IP size
- `master_down_IP` - FDD master down IP

### Available `show`/`display` parameters:
- `config` - Current configuration
- `tasks` - FreeRTOS task list
- `memory` - Memory usage
- `dhcp` or `DHCP_ARP` - DHCP/ARP table

## Examples

### Basic Configuration
```
> set callsign STATION1
Callsign set to STATION1
ready> set network_id 5
Network ID set to 5
ready> set frequency 437.000
Frequency set to 437.000 MHz
ready> set is_master yes
Master mode enabled
ready> radio on
Radio is now ON.
ready> save
Configuration save not yet implemented.
ready> 
```

### Monitoring
```
> status
Modem Status:
  Mode: Master
  Radio: ON
  Client ID: 0
  Connection: Connected
  Telnet Sessions: 5
  Commands: 42
  Uptime: 3600 sec
ready> who
Master/Client Information:
  Mode: MASTER
  Client[0]: STATION1
  Client[1]: STATION2
ready> show dhcp
DHCP/ARP Entries:
  [0] 192.168.1.10  STATION1
  [1] 192.168.1.11  STATION2
ready>
```

### Troubleshooting
```
> show tasks
FreeRTOS Tasks (10):
  RadioISR        Pri:5 Stack:128
  RadioProc       Pri:4 Stack:256
  ...
ready> show memory
Memory Usage:
  Heap Free: 8192 bytes
  Heap Min:  7456 bytes
  RAM Used:  64648 / 65536 (98.9%)
ready> show config
Configuration:
  Network ID: 5
  Frequency: 437.000 MHz
  Mode: Master
  Modulation: 22
  Radio State: ON
ready>
```

## Implementation Details

- **Total Commands**: 18 fully functional commands
- **Flash Usage**: 52,396 bytes (19.9% of 256KB)
- **RAM Usage**: 64,648 bytes (98.9% of 64KB)
- **Code**: ~550 lines in task_telnet.c
- **Features**: Parameter validation, error messages, echo support, line editing

## Notes

- Configuration changes take effect immediately
- Use `save` command to persist changes (when implemented)
- Factory reset via `reset_to_default` clears all settings
- Session timeout: 5 minutes of inactivity
- All commands are case-sensitive
- Multiple aliases supported for common commands (show/display, exit/logout)
