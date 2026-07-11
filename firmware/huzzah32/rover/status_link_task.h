#pragma once

// Core 1: owns Serial2, the status link to the M0 Adalogger (docs/hardware/wiring.md wires
// 5/6, common/status_link.h for the wire protocol). Sends GNSS time sync once time becomes
// valid and periodically thereafter (drift correction); relays button-driven log start/stop
// and safe-shutdown requests; receives the M0's periodic status broadcast into shared.h's
// M0Status (statusGetM0()).
void statusLinkTaskStart();

// Called from ui_task (button gestures) to request a command be sent on the next task
// iteration. Requests are coalesced in simple volatile flags -- fine at human button-press
// rates; no queue needed.
void statusLinkRequestLogCtl(bool start);
void statusLinkRequestShutdown();
