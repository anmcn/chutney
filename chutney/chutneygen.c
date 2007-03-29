#include <string.h>
#include <stdio.h>
#include "chutney.h"

int chutney_dump_init(chutney_dump_state *state, 
                      int (*write)(void *context, const char *s, long n),
                      void *write_context)
{
    state->depth = 0;
    state->write = write;
    state->write_context = write_context;
    return 0;
}

int
chutney_save_stop(chutney_dump_state *self)
{
    static char stop = STOP;

    return self->write(self->write_context, &stop, 1);
}

int
chutney_save_null(chutney_dump_state *self)
{
    static char none = NONE;

    return self->write(self->write_context, &none, 1);
}

int
chutney_save_int(chutney_dump_state *self, long value)
{
    char c_str[32];

    c_str[0] = INT;
    snprintf(c_str + 1, sizeof(c_str) - 1, "%ld\n", value);
    return self->write(self->write_context, c_str, strlen(c_str));
}

