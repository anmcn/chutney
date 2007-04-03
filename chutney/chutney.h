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

    void *(*make_tuple)(void *value, size_t length);
    void *(*make_dict)(void *value, size_t length);
} chutney_creators;

enum chutney_states {
   CHUTNEY_S_OPCODE,            // Looking for an opcode
   CHUTNEY_S_INT_NL,            // collect up to \n, process as INT
   CHUTNEY_S_BININT,            // collect binint bytes, then load
   CHUTNEY_S_BINFLOAT,          // collect binfloat bytes, then load
   CHUTNEY_S_BINSTRING,         // collect N bytes, then load
   CHUTNEY_S_BINSTRING_LEN,     // collect binstring byte count, then load
   CHUTNEY_S_BINUNICODE,        // collect N bytes, then load
   CHUTNEY_S_BINUNICODE_LEN,    // collect binunicode byte count, then load
};

typedef struct {
    chutney_creators creators;
    enum chutney_states parser_state;
    void **stack;
    int stack_alloc;
    int stack_size;
    char *buf;
    int buf_len;                // how many bytes are in the buffer
    int buf_alloc;              // how many bytes of space we've allocated
    int buf_want;               // how many bytes we're looking for
} chutney_load_state;

typedef struct {
    long depth;     // Recursion depth - not used by lib, available for user
    int (*write)(void *context, const char *s, long n);
    void *write_context;
} chutney_dump_state;

enum chutney_status {
    CHUTNEY_OKAY = 0,
    CHUTNEY_CONTINUE = 1,
    CHUTNEY_NOMEM = -1,
    CHUTNEY_PARSE_ERR = -2,
    CHUTNEY_STACK_ERR = -3,
    CHUTNEY_OPCODE_ERR = -4,
};

extern int chutney_load_init(chutney_load_state *state,
                             chutney_creators *creators); 
extern void chutney_load_dealloc(chutney_load_state *state); 
extern enum chutney_status chutney_load(chutney_load_state *state, 
                                        const char **data, int *len);
extern void *chutney_load_result(chutney_load_state *state);

extern int chutney_dump_init(chutney_dump_state *state, 
                      int (*write)(void *context, const char *s, long n),
                      void *write_context);

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
