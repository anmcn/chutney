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
    state->marks = NULL;
    state->marks_alloc = 0;
    state->marks_size = 0;
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
    free(state->marks);
    state->marks = NULL;
    if (state->buf)
        free(state->buf);
    state->buf = NULL;
}

void
stack_dealloc(chutney_load_state *state, void **values, long count)
{
    long i;

    for (i = 0; i < count; ++i)
        state->creators.dealloc(values[i]);
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
mark_push(chutney_load_state *state)
{
    int alloc, *marks;
    if (state->marks_alloc == state->marks_size) {
        alloc = state->marks_alloc + 20;
        if (state->marks)
            marks = (int *)malloc(alloc * sizeof(int));
        else
            marks = (int *)realloc(state->marks, alloc * sizeof(int));
        if (!marks)
            return -1;
        state->marks = marks;
        state->marks_alloc = alloc;
    }
    state->marks[state->marks_size++] = state->stack_size;
    return 0;
}

static int
mark_pop(chutney_load_state *state)
{
    if (!state->marks_size)
        return -1;
    return state->marks[--state->marks_size];
}

static enum chutney_status
stack_pop_mark(chutney_load_state *state, void ***items, long *count)
{
    long mark;

    if ((mark = mark_pop(state)) < 0)
        return CHUTNEY_NOMARK_ERR;
    *items = &state->stack[mark];
    *count = state->stack_size - mark;
    state->stack_size = mark;
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
    return *objp ? CHUTNEY_OKAY : CHUTNEY_NOMEM;
}

static long
parse_binint(chutney_load_state *state)
{
    long l = 0;
    int i;

    for (i = 0; i < state->buf_len; ++i)
        l |= (long)(unsigned char)state->buf[i] << (i * 8);
#if LONG_MAX > 2147483647
    if (state->buf_len == 4 && l & (1L << 31))
        l |= (~0L) << 32;
#endif
    return l;
}

static enum chutney_status
load_binint(chutney_load_state *state, void **objp)
{
    *objp = state->creators.make_int(parse_binint(state));
    return *objp ? CHUTNEY_OKAY : CHUTNEY_NOMEM;
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
    return *objp ? CHUTNEY_OKAY : CHUTNEY_NOMEM;
}

static enum chutney_status
load_tuple(chutney_load_state *state, void **objp)
{
    void **values = NULL;
    long count = 0;
    static enum chutney_status err = CHUTNEY_OKAY;

    err = stack_pop_mark(state, &values, &count);
    if (err != CHUTNEY_OKAY)
        return err;
    *objp = state->creators.make_tuple(values, count);
    return *objp ? CHUTNEY_OKAY : CHUTNEY_NOMEM;
}

static enum chutney_status
dict_setitems(chutney_load_state *state)
{
    void **values = NULL;
    long count = 0;
    void *dict;
    static enum chutney_status err;

    err = stack_pop_mark(state, &values, &count);
    if (err != CHUTNEY_OKAY)
        return err;
    /* Odd number of items => unpaired key/value */
    /* Empty stack => no dictionary */
    if (count & 1 || !state->stack_size) {
        stack_dealloc(state, values, count);
        return CHUTNEY_PARSE_ERR;
    }
    dict = state->stack[state->stack_size - 1];
    if (state->creators.dict_setitems(dict, values, count) < 0)
        return CHUTNEY_NOMEM;
    else
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
            case MARK:
                if (mark_push(state) < 0)
                    return CHUTNEY_NOMEM;
                break;
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
            case SHORT_BINSTRING:
                state->parser_state = CHUTNEY_S_BINSTRING_LEN;
                state->buf_want = 1;
                break;
            case BINSTRING:
                state->parser_state = CHUTNEY_S_BINSTRING_LEN;
                state->buf_want = 4;
                break;
            case BINUNICODE:
                state->parser_state = CHUTNEY_S_BINUNICODE_LEN;
                state->buf_want = 4;
                break;
            case TUPLE:
                err = load_tuple(state, &obj);
                if (err == CHUTNEY_OKAY)
                    err = stack_push(state, obj);
                break;
            case EMPTY_DICT:
                obj = state->creators.make_empty_dict();
                err = stack_push(state, obj);
                break;
            case SETITEMS:
                err = dict_setitems(state);
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
        case CHUTNEY_S_BINSTRING_LEN:
        case CHUTNEY_S_BINSTRING:
        case CHUTNEY_S_BINUNICODE_LEN:
        case CHUTNEY_S_BINUNICODE:
            buf_putc(state, c);
            if (state->buf_len == state->buf_want) {
                obj = NULL;
                switch (state->parser_state) {
                case CHUTNEY_S_BININT:
                    err = load_binint(state, &obj);
                    state->parser_state = CHUTNEY_S_OPCODE;
                    break;
                case CHUTNEY_S_BINFLOAT:
                    err = load_binfloat(state, &obj);
                    state->parser_state = CHUTNEY_S_OPCODE;
                    break;
                case CHUTNEY_S_BINSTRING_LEN:
                    state->buf_want = parse_binint(state);
                    if (!state->buf_want) {
                        obj = state->creators.make_string("", 0);
                        state->parser_state = CHUTNEY_S_OPCODE;
                    } else
                        state->parser_state = CHUTNEY_S_BINSTRING;
                    break;
                case CHUTNEY_S_BINSTRING:
                    obj = state->creators.make_string(state->buf, 
                                                      state->buf_len);
                    if (!obj)
                        err = CHUTNEY_NOMEM;
                    state->parser_state = CHUTNEY_S_OPCODE;
                    break;
                case CHUTNEY_S_BINUNICODE_LEN:
                    state->buf_want = parse_binint(state);
                    if (!state->buf_want) {
                        obj = state->creators.make_unicode("", 0);
                        state->parser_state = CHUTNEY_S_OPCODE;
                    } else
                        state->parser_state = CHUTNEY_S_BINUNICODE;
                    break;
                case CHUTNEY_S_BINUNICODE:
                    obj = state->creators.make_unicode(state->buf, 
                                                       state->buf_len);
                    if (!obj)
                        err = CHUTNEY_NOMEM;
                    state->parser_state = CHUTNEY_S_OPCODE;
                    break;
                default:                /* coding error */
                    return CHUTNEY_PARSE_ERR;
                }
                state->buf_len = 0;
                if (err == CHUTNEY_OKAY && obj)
                    err = stack_push(state, obj);
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
