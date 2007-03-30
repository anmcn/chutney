#include <stdlib.h>
#include <errno.h>
#include "chutney.h"
#include "chutneyprotocol.h"

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

static int stack_push(chutney_load_state *state, void *obj)
{
    if (!obj)
        return -1;
    if (state->stack_size == state->stack_alloc)
        if (stack_grow(state) < 0)
            return -1;
    state->stack[state->stack_size++] = obj;
    return 0;
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

enum chutney_status 
chutney_load(chutney_load_state *state, const char **datap, int *len)
{
    char c;
    int err = 0;
    void *obj;

    while (!err && (*len)--) {
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
            default:
                return CHUTNEY_OPCODE_ERR;
            }
            break;
        case CHUTNEY_S_INT_NL:
            if (c != '\n')
                buf_putc(state, c);
            else {
                buf_putc(state, '\0'); --state->buf_len;
                switch (state->parser_state) {
                case CHUTNEY_S_INT_NL:
                    {
                        long l;
                        char *end;
                        errno = 0;
                        l = strtol(state->buf, &end, 0);
                        if (errno || *end != '\0')
                            return CHUTNEY_PARSE_ERR;
                        obj = state->creators.make_int(l);
                    }
                    break;
                default:                /* coding error */
                    return CHUTNEY_PARSE_ERR;
                }
                state->buf_len = 0;
                err = stack_push(state, obj);
                state->parser_state = CHUTNEY_S_OPCODE;
            }
            break;
        }
    }
    return err ? CHUTNEY_NOMEM : CHUTNEY_CONTINUE;
}

void *
chutney_load_result(chutney_load_state *state)
{
    return state->stack_size == 1 ? state->stack[0] : NULL;
}
