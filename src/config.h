#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>


enum ps_exit {
    PS_EXIT_OK = 0,
    PS_EXIT_CONFIG = 2,
    PS_EXIT_STARTUP = 3,
    PS_EXIT_RUNTIME = 4
};

/* Which subcommand was given on the command line. */
enum ps_command {
    PS_CMD_HELP,
    PS_CMD_LIST,
    PS_CMD_RUN
};

/* Which terminal may transmit to the board. The other one is an observer:
   anything it sends is read and discarded. */
enum ps_writer {
    PS_WRITER_A,
    PS_WRITER_B,
    PS_WRITER_NONE
};

/* Which virtual-port driver behavior to assume. VSPE supplies peer
   connect/disconnect notifications; GENERIC assumes nothing extra. */
enum ps_provider {
    PS_PROVIDER_VSPE,
    PS_PROVIDER_GENERIC

    
};

/* Buffer size for a normalized device path such as L"\\\\.\\COM255". */
#define PS_PATH_CAP 32

/* Highest COM port number accepted (project limit). */
#define PS_COM_PORT_MAX 255u

/* Per-queue byte limits (IMPLEMENTATION_PLAN section 5). */
#define PS_QUEUE_BYTES_MIN     4096u
#define PS_QUEUE_BYTES_MAX     (16u * 1024u * 1024u)
#define PS_QUEUE_BYTES_DEFAULT 65536u

/* Physical-port write timeout limits, in milliseconds. */
#define PS_WRITE_TIMEOUT_MS_MIN     1u
#define PS_WRITE_TIMEOUT_MS_MAX     600000u
#define PS_WRITE_TIMEOUT_MS_DEFAULT 5000u

/* Fully validated result of parsing the command line. */
struct ps_config {
    enum ps_command  command;

    /* Normalized device paths, e.g. L"\\\\.\\COM5". Empty when not given. */
    wchar_t          source_path[PS_PATH_CAP];
    wchar_t          client_a_path[PS_PATH_CAP];
    wchar_t          client_b_path[PS_PATH_CAP];

    enum ps_writer   writer;
    uint32_t         baud;
    enum ps_provider provider;
    uint32_t         queue_bytes;
    uint32_t         write_timeout_ms;

    bool             dtr;                      /* --dtr on|off, default off */
    bool             rts;                      /* --rts on|off, default off */
    bool             discard_source_on_start;  /* --discard-source-input-on-start */
    bool             wait_for_start;           /* start barrier, default on */
};

/* Resets *cfg to all defaults. Required run options are left empty/zero
   and must be supplied by the parser. */
void ps_config_set_defaults(struct ps_config *cfg);



