#pragma once

#include "../HID_Common.hpp"

bool SerialReady();
void initSerial();
void beforeSerialRead();
void afterSerialRead();
HIDControls readSerial();

