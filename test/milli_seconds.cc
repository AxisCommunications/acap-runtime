/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "milli_seconds.h"

uint64_t MilliSeconds()
{
  return duration_cast<milliseconds>(
    system_clock::now().time_since_epoch()).count();
}