#include <sys/types.h>

/*
 * Pickle opcodes from Python Modules/cPickle.c 
 */
#define MARK        '('
#define STOP        '.'
#define POP         '0'
#define POP_MARK    '1'
#define DUP         '2'
#define FLOAT       'F'
#define BINFLOAT    'G'
#define INT         'I'
#define BININT      'J'
#define BININT1     'K'
#define LONG        'L'
#define BININT2     'M'
#define NONE        'N'
#define PERSID      'P'
#define BINPERSID   'Q'
#define REDUCE      'R'
#define STRING      'S'
#define BINSTRING   'T'
#define SHORT_BINSTRING 'U'
#define UNICODE     'V'
#define BINUNICODE  'X'
#define APPEND      'a'
#define BUILD       'b'
#define GLOBAL      'c'
#define DICT        'd'
#define EMPTY_DICT  '}'
#define APPENDS     'e'
#define GET         'g'
#define BINGET      'h'
#define INST        'i'
#define LONG_BINGET 'j'
#define LIST        'l'
#define EMPTY_LIST  ']'
#define OBJ         'o'
#define PUT         'p'
#define BINPUT      'q'
#define LONG_BINPUT 'r'
#define SETITEM     's'
#define TUPLE       't'
#define EMPTY_TUPLE ')'
#define SETITEMS    'u'

/* Protocol 2. */
#define PROTO	 '\x80' /* identify pickle protocol */
#define NEWOBJ   '\x81' /* build object by applying cls.__new__ to argtuple */
#define EXT1     '\x82' /* push object from extension registry; 1-byte index */
#define EXT2     '\x83' /* ditto, but 2-byte index */
#define EXT4     '\x84' /* ditto, but 4-byte index */
#define TUPLE1   '\x85' /* build 1-tuple from stack top */
#define TUPLE2   '\x86' /* build 2-tuple from two topmost stack items */
#define TUPLE3   '\x87' /* build 3-tuple from three topmost stack items */
#define NEWTRUE  '\x88' /* push True */
#define NEWFALSE '\x89' /* push False */
#define LONG1    '\x8a' /* push long from < 256 bytes */
#define LONG4    '\x8b' /* push really big long */

/* There aren't opcodes -- they're ways to pickle bools before protocol 2,
 * so that unpicklers written before bools were introduced unpickle them
 * as ints, but unpicklers after can recognize that bools were intended.
 * Note that protocol 2 added direct ways to pickle bools.
 */
#undef TRUE
#define TRUE        "I01\n"
#undef FALSE
#define FALSE       "I00\n"


#define BATCHSIZE 1000

typedef struct {
    void (*make_null)(void);

    void (*make_int)(int value);
    void (*make_float)(int value);
    void (*make_string)(char *value, size_t length);
    void (*make_unicode)(char *value, size_t length);

    void (*make_tuple)(void *value, size_t length);
    void (*make_dict)(void *value, size_t length);
} creators_t;

typedef struct {
    long depth;                         // Recursion depth
    int (*write)(void *context, const char *s, long n);
    void *write_context;
} chutney_dump_state;

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
