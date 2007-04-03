#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "chutney.h"
#include "chutneyprotocol.h"
#include "chutneyutil.h"

#define STACK_POP(S) \
    ((S)->stack_size ? (S)->stack[--((S)->stack_size)] : (void *)0)

int
chutney_load_init(chutney_load_state *state, chutney_creators *creators)
{
    state->parser_state = CHUTNEY_S_OPCODE;
    state->creators = *creators;
    state->stack_size = 0;
    state->stack_alloc = 256;
    if (!(state->stack = malloc(state->stack_alloc * sizeof(void *))))
        return -1;
    state->buf_len = 0;
    state->buf_alloc = 0;
    state->buf = NULL;
    return 0;
}

void
chutney_load_dealloc(chutney_load_state *state)
{
    void *obj;

    while ((obj = STACK_POP(state)))
        state->creators.dealloc(obj);
    free(state->stack);
    state->stack = NULL;
    if (state->buf)
        free(state->buf);
    state->buf = NULL;
}

static int stack_grow(chutney_load_state *state)
{
    void *tmp;
    int bigger;
    size_t nbytes;

    bigger = state->stack_alloc << 1;
    if (bigger <= 0)
        return -1;
    if ((int)(size_t)bigger != bigger)
        return -1;
    nbytes = (size_t)bigger * sizeof(void *);
    if (nbytes / sizeof(void *) != (size_t)bigger)
        return -1;
    tmp = realloc(state->stack, nbytes);
    if (!tmp)
        return -1;
    state->stack = tmp;
    state->stack_alloc = bigger;
    return 0;
}

static enum chutney_status
stack_push(chutney_load_state *state, void *obj)
{
    if (!obj)
        return CHUTNEY_NOMEM;
    if (state->stack_size == state->stack_alloc)
        if (stack_grow(state) < 0)
            return CHUTNEY_NOMEM;
    state->stack[state->stack_size++] = obj;
    return CHUTNEY_OKAY;
}

static int
buf_grow(chutney_load_state *state) {
    char *tmp;
    int bigger;

    if (!state->buf_alloc) {
        bigger = 256;
        tmp = malloc(bigger);
    } else {
        bigger = state->buf_alloc << 1;
        if (bigger <= 0)
            return -1;
        if ((int)(size_t)bigger != bigger)
            return -1;
        tmp = realloc(state->buf, bigger);
    }
    if (!tmp)
        return -1;
    state->buf = tmp;
    state->buf_alloc = bigger;
    return 0;
}

static int
buf_putc(chutney_load_state *state, char c)
{
    if (state->buf_alloc == state->buf_len)
        if (buf_grow(state) < 0)
            return -1;
    state->buf[state->buf_len++] = c;
    return 0;
}

static enum chutney_status
load_int(chutney_load_state *state, void **objp)
{
    long l;
    char *end;

    errno = 0;
    l = strtol(state->buf, &end, 0);
    if (errno || *end != '\0')
        return CHUTNEY_PARSE_ERR;
    *objp = state->creators.make_int(l);
    return CHUTNEY_OKAY;
}

static enum chutney_status
load_binint(chutney_load_state *state, void **objp)
{
    long l = 0;
    int i;

    for (i = 0; i < state->buf_len; ++i)
        l |= (long)(unsigned char)state->buf[i] << (i * 8);
#if LONG_MAX > 2147483647
    if (state->buf_len == 4 && l & (1L << 31))
        l |= (~0L) << 32;
#endif
    *objp = state->creators.make_int(l);
    return CHUTNEY_OKAY;
}

static enum chutney_status
load_binfloat(chutney_load_state *state, void **objp)
{
    double l;
    char buf[8], *q;
    int i;

    if (state->buf_len != sizeof(double))
        return CHUTNEY_PARSE_ERR;
    switch (detect_ieee_fp()) {
    case IEEE_LE:
        for (i = 0, q = &buf[sizeof(buf)]; i < sizeof(buf); ++i)
            *--q = state->buf[i];
        l = *(double *)buf;
        break;
    case IEEE_BE:
        l = *(double *)state->buf;
        break;
    default:
        return CHUTNEY_PARSE_ERR;
    }
    *objp = state->creators.make_float(l);
    return CHUTNEY_OKAY;
}

enum chutney_status 
chutney_load(chutney_load_state *state, const char **datap, int *len)
{
    char c;
    enum chutney_status err = CHUTNEY_OKAY;
    void *obj = NULL;

    while (err == CHUTNEY_OKAY && (*len)--) {
        c = *(*datap)++;
        switch (state->parser_state) {
        case CHUTNEY_S_OPCODE:
            switch (c) {
            case STOP:
                /* if stack empty, raise an error */
                if (state->stack_size != 1)
                    return CHUTNEY_STACK_ERR;
                return CHUTNEY_OKAY;
            case NONE:
                err = stack_push(state, state->creators.make_null());
                break;
            case NEWTRUE:
            case NEWFALSE:
                obj = state->creators.make_bool(c == NEWTRUE);
                err = stack_push(state, obj);
                break;
            case INT:
                state->parser_state = CHUTNEY_S_INT_NL;
                break;
            case BININT:
                state->parser_state = CHUTNEY_S_BININT;
                state->buf_want = 4;
                break;
            case BININT2:
                state->parser_state = CHUTNEY_S_BININT;
                state->buf_want = 2;
                break;
            case BINFLOAT:
                state->parser_state = CHUTNEY_S_BINFLOAT;
                state->buf_want = 8;
                break;
            default:
                return CHUTNEY_OPCODE_ERR;
            }
            break;

        /* collect bytes until newline */
        case CHUTNEY_S_INT_NL:
            if (c != '\n')
                buf_putc(state, c);
            else {
                buf_putc(state, '\0'); --state->buf_len;
                switch (state->parser_state) {
                case CHUTNEY_S_INT_NL:
                    err = load_int(state, &obj);
                    break;
                default:                /* coding error */
                    return CHUTNEY_PARSE_ERR;
                }
                state->buf_len = 0;
                if (!err)
                    err = stack_push(state, obj);
                state->parser_state = CHUTNEY_S_OPCODE;
            }
            break;

        /* collect want_buf bytes */
        case CHUTNEY_S_BININT:
        case CHUTNEY_S_BINFLOAT:
            buf_putc(state, c);
            if (state->buf_len == state->buf_want) {
                switch (state->parser_state) {
                case CHUTNEY_S_BININT:
                    err = load_binint(state, &obj);
                    break;
                case CHUTNEY_S_BINFLOAT:
                    err = load_binfloat(state, &obj);
                    break;
                default:                /* coding error */
                    return CHUTNEY_PARSE_ERR;
                }
                state->buf_len = 0;
                if (!err)
                    err = stack_push(state, obj);
                state->parser_state = CHUTNEY_S_OPCODE;
            }
            break;
        }
    }
    return err != CHUTNEY_OKAY ? err : CHUTNEY_CONTINUE;
}

void *
chutney_load_result(chutney_load_state *state)
{
    return state->stack_size == 1 ? state->stack[0] : NULL;
}
