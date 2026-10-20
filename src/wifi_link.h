#pragma once

#include <stdint.h>

// Brings Wi-Fi up and waits up to timeout_ms for a connection.
bool wifi_up(uint32_t timeout_ms);
// Disconnects and powers the radio's Wi-Fi side down.
void wifi_down();
bool wifi_is_connected();
