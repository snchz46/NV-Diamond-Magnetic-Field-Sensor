#pragma once
#include <Arduino.h>
#include "nv_signal.h"

void csv_push(const NvSample& s);
void csv_write_to_client(class WebServer& server);
