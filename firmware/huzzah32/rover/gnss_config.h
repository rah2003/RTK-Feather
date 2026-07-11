#pragma once
#include <SparkFun_u-blox_GNSS_v3.h>

// Applies the full project configuration to the ZED-F9P over UART1 -- the only transport
// Topology B has (docs/hardware/topology.md: the simpleRTK2B Lite exposes no I2C pads).
// Idempotent (VALSET with the same values is a no-op for the receiver), layers = RAM+BBR+
// Flash so it survives a power cycle. Called once at boot by gnssTask (the only owner of the
// GNSS link) and by the serial menu's factory-recovery command.
bool gnssApplyProjectConfig(SFE_UBLOX_GNSS_SERIAL& gnss);

// "Restore factory + apply project config" recovery: UBX-CFG-CFG factory reset, wait for the
// module to come back, then gnssApplyProjectConfig().
bool gnssFactoryRecover(SFE_UBLOX_GNSS_SERIAL& gnss);
