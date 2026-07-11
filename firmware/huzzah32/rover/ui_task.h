#pragma once

// Core 1: polls the OLED-Wing buttons (A/B/C) and turns gestures into status-link requests.
// See ui_task.cpp for the button mapping -- it's a PROPOSAL (project brief §9's usability
// suggestions are "propose, don't silently implement"), adapted from the brief's generic
// 2-button pattern to this build's actual 3 buttons. Flag if you want it changed.
void uiTaskStart();

constexpr int kUiNumPages = 4;  // fix / NTRIP / logging / BLE -- project brief §3.5
int uiCurrentPage();            // which page A-short-press has cycled to (0..kUiNumPages-1)
