#pragma once
#include <Arduino.h>

bool web_init();
void web_loop();

// called by main to publish samples
void web_publish_sample(float raw, float filtered, float delta, bool dip, float rpm);
