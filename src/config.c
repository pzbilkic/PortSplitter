#include "config.h"

void ps_config_set_defaults(struct ps_config *cfg)
{
    *cfg = (struct ps_config){0};

    cfg->command          = PS_CMD_HELP;
    cfg->writer           = PS_WRITER_NONE;
    cfg->provider         = PS_PROVIDER_GENERIC;
    cfg->queue_bytes      = PS_QUEUE_BYTES_DEFAULT;
    cfg->write_timeout_ms = PS_WRITE_TIMEOUT_MS_DEFAULT;
    cfg->wait_for_start   = true;
}
