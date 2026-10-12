# SFG SBS

A C++17 library for interacting with Smart Battery System (SBS) compliant batteries over I2C/SMBus. It supports both host and battery modes. The implementation is based on the [Smart Battery Data Specification Rev 1.1](https://sbs-forum.org/specs/sbdat110.pdf) and the [System Management Bus Specification Rev 1.1](https://smbus.org/specs/smbus110.pdf).

## Features

- [x] **Core Command Set:** Definitions and data traits for standard SBS functions are fully mapped.
- [x] **Host-to-Battery Reads:** Implementation of the SMBus read word/block protocols.
- [x] **Battery Emulation:** Framework for acting as a smart battery and responding to host requests.
- [ ] **Bus Timeouts:** A timeout mechanism aligning with the maximum clock-low limit.
- [x] **Host-to-Battery Writes:** SMBus write word/block protocols to set battery parameters.
- [x] **Critical Message Broadcasts:** Logic for the battery to act as a bus master and broadcast `AlarmWarning()`.
- [ ] **Charger Broadcasts:** Logic for the battery to broadcast `ChargingCurrent()` and `ChargingVoltage()` to a smart charger address.
- [ ] **Packet Error Checking:** The optional SMBus v1.1 mechanism to validate data integrity using an appended checksum byte.
- [ ] **Capacity Mode Scaling:** Dynamic unit conversions based on the `CAPACITY_MODE` bit in the `BatteryMode()` register.
- [ ] **Safety Signal:** Physical implementation of the variable-resistance T-pin required for fail-safe charging.

## Building and Uploading

This project features a VS Code devcontainer and uses PlatformIO. Run `pio run --list-targets` to see the available environments and build one with `pio run -e <env>`. If you want to upload the firmware to a connected board, use `pio run -e <env> -t upload`.

## Testing

Run tests via `pio test -e native_test`.

> [!note]
> Every push runs `pio test -e native_test` plus all example firmware builds.