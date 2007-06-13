#include <sys/types.h>

#define CHUTNEY_BATCHSIZE 1000

typedef struct {
    void (*dealloc)(void *value);

    void *(*make_null)(void);

    void *(*make_bool)(int value);
    void *(*make_int)(int value);
    void *(*make_float)(double value);
    void *(*make_string)(const char *value, long length);
    void *(*make_unicode)(const char *value, long length);

    void *(*make_tuple)(void **values, long count);
    void *(*make_empty_dict)(void);
    int (*dict_setitems)(void *dict, void **values, long count);

    void *(*get_global)(const char *module, const char *name);
    void *(*make_object)(void *cls);
    int (*object_build)(void *obj, void *state);
} chutney_load_callbacks;

enum chutney_states {
   CHUTNEY_S_OPCODE,            // Looking for an opcode
   // Following states call state->completion
   CHUTNEY_S_BUF_NL,            // collect up to \n
   CHUTNEY_S_BUF_CNT,           // collect want_cnt bytes
};

enum chutney_status {
    CHUTNEY_OKAY = 0,
    CHUTNEY_CONTINUE = 1,
    CHUTNEY_NOMEM = -1,
    CHUTNEY_PARSE_ERR = -2,
    CHUTNEY_STACK_ERR = -3,
    CHUTNEY_OPCODE_ERR = -4,
    CHUTNEY_NOMARK_ERR = -5,
    CHUTNEY_CALLBACK_ERR = -6,
};

typedef struct {
    char *module;
    char *name;
} chutney_op_global;

typedef struct chutney_load_state {
    chutney_load_callbacks callbacks;
    enum chutney_states parser_state;
    void **stack;
    int stack_alloc;
    int stack_size;
    int *marks;                 // MARK stack
    int marks_alloc;
    int marks_size;
    char *buf;
    int buf_len;                // how many bytes are in the buffer
    int buf_alloc;              // how many bytes of space we've allocated
    int buf_want;               // how many bytes we're looking for
    enum chutney_status (*completion)(struct chutney_load_state *state);
                                // Some states call this on completion of their
                                // action.
    union {
        chutney_op_global global;
    } op_state;                 // Opcode specific state
} chutney_load_state;
typedef enum chutney_status (*completion_fn)(struct chutney_load_state *);

typedef struct {
    long depth;     // Recursion depth - not used by lib, available for user
    int (*write)(void *context, const char *s, long n);
    void *write_context;
} chutney_dump_state;

/* Load function */
extern int chutney_load_init(chutney_load_state *state,
                             chutney_load_callbacks *callbacks); 
extern void chutney_load_dealloc(chutney_load_state *state); 
extern enum chutney_status chutney_load(chutney_load_state *state, 
                                        const char **data, int *length);
extern void *chutney_load_result(chutney_load_state *state);

/* Dump functions */
extern int chutney_dump_init(chutney_dump_state *state, 
                      int (*write)(void *context, const char *s, long n),
                      void *write_context);
extern void chutney_dump_dealloc(chutney_dump_state *state);

extern int chutney_save_stop(chutney_dump_state *self);
extern int chutney_save_mark(chutney_dump_state *self);
extern int chutney_save_null(chutney_dump_state *self);
extern int chutney_save_bool(chutney_dump_state *self, int value);
extern int chutney_save_int(chutney_dump_state *self, long value);
extern int chutney_save_float(chutney_dump_state *self, double value);
extern int chutney_save_string(chutney_dump_state *self, 
                                const char *value, int size);
extern int chutney_save_utf8(chutney_dump_state *self, 
                                const char *value, int size);
extern int chutney_save_tuple(chutney_dump_state *self);
extern int chutney_save_empty_dict(chutney_dump_state *self);
extern int chutney_save_setitems(chutney_dump_state *self);
extern int chutney_save_global(chutney_dump_state *self, 
                                const char *module, const char *name);
extern int chutney_save_obj(chutney_dump_state *self);
extern int chutney_save_build(chutney_dump_state *self);
