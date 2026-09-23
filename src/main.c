/*
Main.c serial port splitter
By: Petar Bilkic
*/

#include <stdio.h> 
#include <wchar.h> 

#include "config.h"

int wmain(int argc, wchar_t **argv)
{
    (void)argc; // Unused parameter
    (void)argv; // Unused parameter

    struct ps_config cfg;
    ps_config_set_defaults(&cfg);

    wprintf(L"portsplitter 0.1.0\n");
    wprintf(L"queue_bytes=%u write_timeout_ms=%u\n", cfg.queue_bytes, cfg.write_timeout_ms);
    return 0;
}