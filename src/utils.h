#pragma once
#include "windows_framework.h"

#define KB(x) ((x) * 1024ll)
#define COUNTOF(x) (sizeof(x) / sizeof((x)[0]))

void DebugLog(LPCWSTR format, ...)
{
    va_list args;
    va_start(args, format);
    WCHAR szBuffer[512]; // get rid of this hard-coded buffer
    vswprintf_s(szBuffer, format, args);
    OutputDebugStringW(szBuffer);
    va_end(args);
}
