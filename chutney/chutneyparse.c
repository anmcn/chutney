#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include "chutney.h"
#include "chutneyprotocol.h"
#include "chutneyutil.h"

#define STACK_POP(S) \
    ((S)->stack_size ? (S)->stack[--((S)->stack_size)] : (void *)0)

int
chutney_load_init(chutney_load_state *state, chutney_load_callbacks *callbacks)
{
    assert(callbacks->dealloc != NULL);
    assert(callbacks->make_null != NULL);
    assert(callbacks->make_bool != NULL);
    assert(callbacks->make_int != NULL);
    assert(callbacks->make_float != NULL);
    assert(callbacks->make_string != NULL);
    assert(callbacks->make_unicode != NULL);
    assert(callbacks->make_tuple != NULL);
    assert(callbacks->make_empty_dict != NULL);
    assert(callbacks->dict_setitems != NULL);
    assert(callbacks->get_global != NULL);
    assert(callbacks->make_object != NULL);
    assert(callbacks->object_build != NULL);

    state->parser_state = CHUTNEY_S_OPCODE;
    state->callbacks = *callbacks;
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
    state->completion = NULL;
    return 0;
}

void
chutney_load_dealloc(chutney_load_state *state)
{
    void *obj;

    while ((obj = STACK_POP(state)))
        state->callbacks.dealloc(obj);
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
        state->callbacks.dealloc(values[i]);
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
    if (obj == NULL)
        return CHUTNEY_CALLBACK_ERR;
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

/*
 * Return a malloc'ed copy of the current buffer. The storage pointed to by
 * *copy must be free()'ed.
 */
static enum chutney_status
buf_dupe(chutney_load_state *state, char **copy)
{
    if ((*copy = malloc(state->buf_len + 1)) == NULL)
        return CHUTNEY_NOMEM;
    memcpy(*copy, state->buf, state->buf_len + 1);
    return CHUTNEY_OKAY;
}

/*
 * Set up the state machine to read bytes into buf up to the next \n, and then
 * call the given /completion/ function.
 */
static void
state_buf_nl(chutney_load_state *state, 
             int (*completion)(chutney_load_state *state))
{
    assert(state->completion == NULL);
    state->parser_state = CHUTNEY_S_BUF_NL;
    state->completion = completion;
}

/*
 * Set up the state machine read /count/ bytes into buf, and then call the
 * given /completion/ function.
 */
static void
state_buf_count(chutney_load_state *state, int count, 
                int (*completion)(chutney_load_state *state))
{
    assert(state->completion == NULL);
    state->parser_state = CHUTNEY_S_BUF_CNT;
    state->buf_want = count;
    state->completion = completion;
}

static enum chutney_status
load_int(chutney_load_state *state)
{
    long l;
    char *end;

    errno = 0;
    l = strtol(state->buf, &end, 0);
    if (errno || *end != '\0')
        return CHUTNEY_PARSE_ERR;
    return stack_push(state, state->callbacks.make_int(l));
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
load_binint(chutney_load_state *state)
{
    return stack_push(state, state->callbacks.make_int(parse_binint(state)));
}

static enum chutney_status
load_binfloat(chutney_load_state *state)
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
    return stack_push(state, state->callbacks.make_float(l));
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
    *objp = state->callbacks.make_tuple(values, count);
    return *objp ? CHUTNEY_OKAY : CHUTNEY_CALLBACK_ERR;
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
    if (state->callbacks.dict_setitems(dict, values, count) < 0)
        return CHUTNEY_CALLBACK_ERR;
    else
        return CHUTNEY_OKAY;
}

static enum chutney_status
load_binstring(struct chutney_load_state *state)
{
    return stack_push(state, state->callbacks.make_string(state->buf, 
                                                         state->buf_len));
}


static enum chutney_status
s_binstring(struct chutney_load_state *state)
{
    int want = parse_binint(state);

    if (!want)
        return stack_push(state, state->callbacks.make_string("", 0));
    state_buf_count(state, want, load_binstring);
    return CHUTNEY_OKAY;
}

static enum chutney_status
load_binunicode(struct chutney_load_state *state)
{
    return stack_push(state, state->callbacks.make_unicode(state->buf, 
                                                          state->buf_len));
}

static enum chutney_status
s_binunicode(struct chutney_load_state *state)
{
    int want = parse_binint(state);

    if (!want)
        return stack_push(state, state->callbacks.make_unicode("", 0));
    state_buf_count(state, want, load_binunicode);
    return CHUTNEY_OKAY;
}

/* Load second argument of GLOBAL opcode, construct "global" object */
static enum chutney_status
load_global(struct chutney_load_state *state)
{
    void *obj;
    chutney_op_global *global = &state->op_state.global;

    if (buf_dupe(state, &state->op_state.global.name) < 0)
        return CHUTNEY_NOMEM;
    obj = state->callbacks.get_global(global->module, global->name);
    free(global->name);
    free(global->module);
    return stack_push(state, obj);
}

/* Load first argument of GLOBAL opcode */
static enum chutney_status
s_global_module(struct chutney_load_state *state)
{
    if (buf_dupe(state, &state->op_state.global.module) < 0)
        return CHUTNEY_NOMEM;
    state_buf_nl(state, load_global);
    return CHUTNEY_OKAY;
}

static enum chutney_status
load_object(chutney_load_state *state)
{
    void **values = NULL;
    long count = 0;
    static enum chutney_status err;

    err = stack_pop_mark(state, &values, &count);
    if (err != CHUTNEY_OKAY)
        return err;
    if (count != 1) {
        stack_dealloc(state, values, count);
        return CHUTNEY_PARSE_ERR;
    }
    return stack_push(state, state->callbacks.make_object(*values));
}

static enum chutney_status
object_build(chutney_load_state *state)
{
    void *obj, *objstate;

    if ((objstate = STACK_POP(state)) == NULL)
        return CHUTNEY_STACK_ERR;
    if ((obj = STACK_POP(state)) == NULL) {
        state->callbacks.dealloc(objstate);
        return CHUTNEY_STACK_ERR;
    }
    if (state->callbacks.object_build(obj, objstate) < 0) {
        state->callbacks.dealloc(obj);
        return CHUTNEY_CALLBACK_ERR;
    }
    return stack_push(state, obj);
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
                err = stack_push(state, state->callbacks.make_null());
                break;
            case NEWTRUE:
            case NEWFALSE:
                obj = state->callbacks.make_bool(c == NEWTRUE);
                err = stack_push(state, obj);
                break;
            case INT:
                state_buf_nl(state, load_int);
                break;
            case BININT:
                state_buf_count(state, 4, load_binint);
                break;
            case BININT2:
                state_buf_count(state, 2, load_binint);
                break;
            case BINFLOAT:
                state_buf_count(state, 8, load_binfloat);
                break;
            case SHORT_BINSTRING:
                state_buf_count(state, 1, s_binstring);
                break;
            case BINSTRING:
                state_buf_count(state, 4, s_binstring);
                break;
            case BINUNICODE:
                state_buf_count(state, 4, s_binunicode);
                break;
            case TUPLE:
                err = load_tuple(state, &obj);
                if (err == CHUTNEY_OKAY)
                    err = stack_push(state, obj);
                break;
            case EMPTY_DICT:
                obj = state->callbacks.make_empty_dict();
                err = stack_push(state, obj);
                break;
            case SETITEMS:
                err = dict_setitems(state);
                break;
            case GLOBAL:
                state_buf_nl(state, s_global_module);
                break;
            case OBJ:
                err = load_object(state);
                break;
            case BUILD:
                err = object_build(state);
                break;
            default:
                return CHUTNEY_OPCODE_ERR;
            }
            break;

        /* collect bytes until newline, then call /completion/ */
        case CHUTNEY_S_BUF_NL:
            if (c != '\n')
                buf_putc(state, c);
            else {
                completion_fn completion = state->completion;
                buf_putc(state, '\0'); --state->buf_len;
                state->completion = NULL;
                state->parser_state = CHUTNEY_S_OPCODE;
                err = completion(state);
                state->buf_len = 0;
            }
            break;

        /* collect want_buf bytes, then call /completion/ */
        case CHUTNEY_S_BUF_CNT:
            buf_putc(state, c);
            if (state->buf_len == state->buf_want) {
                completion_fn completion = state->completion;
                state->completion = NULL;
                state->parser_state = CHUTNEY_S_OPCODE;
                err = completion(state);
                state->buf_len = 0;
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
