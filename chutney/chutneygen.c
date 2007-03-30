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
chutney_save_mark(chutney_dump_state *self)
{
    static char mark = MARK;

    return self->write(self->write_context, &mark, 1);
}

int
chutney_save_null(chutney_dump_state *self)
{
    static char none = NONE;

    return self->write(self->write_context, &none, 1);
}

int 
chutney_save_bool(chutney_dump_state *self, int value)
{
    /* protocol 2 */
    char opcode;
    
    opcode = value ? NEWTRUE : NEWFALSE;
    return self->write(self->write_context, &opcode, 1);
}

int
chutney_save_int(chutney_dump_state *self, long value)
{
    /* protocol 1
    char c_str[6];
    int len = 0;

    c_str[1] = (int)( value        & 0xff);
    c_str[2] = (int)((value >> 8)  & 0xff);
    c_str[3] = (int)((value >> 16) & 0xff);
    c_str[4] = (int)((value >> 24) & 0xff);
    if ((c_str[4] == 0) && (c_str[3] == 0)) {
        c_str[0] = BININT2;
        len = 3;
    } else {
        c_str[0] = BININT;
        len = 5;
    }
    return self->write(self->write_context, c_str, len);
    */
    char c_str[32];

    c_str[0] = INT;
    snprintf(c_str + 1, sizeof(c_str) - 1, "%ld\n", value);
    return self->write(self->write_context, c_str, strlen(c_str));
}

int
chutney_save_float(chutney_dump_state *self, double value)
{
    char c_str[250];

    /* We use the protocol 0 format here, as protocol 1 requires
     * _PyFloat_Pack8, whos implementation is not trivial */
    c_str[0] = FLOAT;
    snprintf(c_str + 1, sizeof(c_str) - 1, "%.17g\n", value);
    return self->write(self->write_context, c_str, strlen(c_str));
}

int
chutney_save_string(chutney_dump_state *self, const char *value, int size)
{
    char c_str[5];
    int len;

    /* We use the protocol 1 here, as protocol 0 requires python repr() of the
     * string */
    if (size < 256) {
        c_str[0] = SHORT_BINSTRING;
        c_str[1] = size;
        len = 2;
    }
    else {
        c_str[0] = BINSTRING;
        c_str[1] = (int)( size        & 0xff);
        c_str[2] = (int)((size >> 8)  & 0xff);
        c_str[3] = (int)((size >> 16) & 0xff);
        c_str[4] = (int)((size >> 24) & 0xff);
        len = 5;
    }
    if (self->write(self->write_context, c_str, len) < 0)
        return -1;
    return self->write(self->write_context, value, size);
}

int
chutney_save_utf8(chutney_dump_state *self, const char *value, int size)
{
    char c_str[5];
    int len;

    /* We use the protocol 1 here, as protocol 0 requires python-specific
     * UTF-8 repr() escaping. */
    c_str[0] = BINUNICODE;
    c_str[1] = (int)( size        & 0xff);
    c_str[2] = (int)((size >> 8)  & 0xff);
    c_str[3] = (int)((size >> 16) & 0xff);
    c_str[4] = (int)((size >> 24) & 0xff);
    len = 5;
    if (self->write(self->write_context, c_str, len) < 0)
        return -1;
    return self->write(self->write_context, value, size);
}

int
chutney_save_tuple(chutney_dump_state *self)
{
    static char tuple = TUPLE;

    /* This creates a tuple from all items on the stack back to the most recent
     * MARK */
    return self->write(self->write_context, &tuple, 1);
}

int
chutney_save_empty_dict(chutney_dump_state *self)
{
    static char empty_dict = EMPTY_DICT;

    return self->write(self->write_context, &empty_dict, 1);
}

int
chutney_save_setitems(chutney_dump_state *self)
{
    static char setitems = SETITEMS;

    /* This adds all pairs of items on the stack up to the the most recent MARK
     * to the dictionary preceeding the MARK */
    return self->write(self->write_context, &setitems, 1);
}

