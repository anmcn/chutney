#include <stdlib.h>
#include "chutney.h"
#include "chutneyprotocol.h"

#define STACKPOP(S) \
    ((S)->stack_size ? (S)->stack[--((S)->stack_size)] : (void *)0)

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

int
chutney_load_init(chutney_load_state *state, chutney_creators *creators)
{
    state->parser_state = CHUTNEY_S_OPCODE;
    state->creators = *creators;
    state->stack_size = 0;
    state->stack_alloc = 256;
    if (!(state->stack = malloc(state->stack_alloc * sizeof(void *))))
        return -1;
    return 0;
}

void
chutney_load_dealloc(chutney_load_state *state)
{
    void *obj;

    while ((obj = STACKPOP(state)))
        state->creators.dealloc(obj);
    free(state->stack);
    state->stack = NULL;
}

enum chutney_status 
chutney_load(chutney_load_state *state, const char **datap, int *len)
{
    char c;

    while ((*len)--) {
        c = *(*datap)++;
        switch (state->parser_state) {
        case CHUTNEY_S_OPCODE:
            switch (c) {
            case STOP:
                /* if stack empty, raise an error */
                if (state->stack_size != 1)
                    return CHUTNEY_ERROR;
                return CHUTNEY_OKAY;
            }
        }
    }
    return CHUTNEY_CONTINUE;
}

void *
chutney_load_result(chutney_load_state *state)
{
    return state->stack_size == 1 ? state->stack[0] : NULL;
}
