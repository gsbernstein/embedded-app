//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESPTask.hpp

    \brief      WiFi ESP32 Wireless driver RTOS Task and Events

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESPDefs.hpp"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define WIFIESP_TASK_CORENUMBER 1 // Core for the WIFIESP Task
#define WIFIESP_TASK_STACKSIZE 4096

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------

/// Post an Event to the task
extern "C" esp_err_t WiFiESPTask_PostEvent(wifiesp_event_t *data);

/// Start the Network Task
extern "C" bool WiFiESPTask_StartNetworkEventTask();

/// Set Event Status Bit
extern "C" int WiFiESPTask_SetEventStatusBits(int bits);

/// Clear Event Status Bit
extern "C" int WiFiESPTask_ClearEventStatusBits(int bits);

/// Get Event Status Bit
extern "C" int WiFiESPTask_GetEventStatusBits();

/// Wait for an Event Status Bit
extern "C" int WiFiESPTask_WaitEventStatusBits(int bits, uint32_t timeout_ms);
