#pragma once

// Core 1: the sole owner of the F9P UART1 link (the SparkFun library isn't thread-safe, so
// exactly one task may call it). Connects, applies project config, injects RTCM from
// rtcmRing, and publishes GnssStatus (fix/position/time + latest GGA for the NTRIP task).
// Does NOT log RAWX/SFRBX -- that's the M0's job over the passive tap (Topology B); this
// task only needs to make sure those messages are turned ON (gnss_config.cpp) so the M0 has
// something to see.
void gnssTaskStart();

// Requested by the serial menu: UBX-CFG-CFG factory reset + reapply project config.
void gnssRequestFactoryRecover();
