#pragma once

// Core 1: polls the OLED-Wing buttons (A/B/C) and turns gestures into status-link requests.
// See ui_task.cpp for the button mapping -- it's a PROPOSAL (project brief §9's usability
// suggestions are "propose, don't silently implement"), adapted from the brief's generic
// 2-button pattern to this build's actual 3 buttons. Flag if you want it changed.
void uiTaskStart();
