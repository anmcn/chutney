#include <Python.h>
#include <cStringIO.h>
#include <limits.h>
#include "chutney.h"

#if (PY_VERSION_HEX < 0x02050000)
typedef int Py_ssize_t;
#endif

static PyObject *ChutneyError, *UnpickleableError, *UnpicklingError;

static void creator_dealloc(void *obj)
{
    Py_DECREF((PyObject *)obj);
}

static void *creator_null(void) {
    Py_INCREF(Py_None);
    return Py_None;
}

static void *creator_bool(int value) {
    PyObject *obj = value ? Py_True : Py_False;
    Py_INCREF(obj);
    return (void *)obj;
}

static void *creator_int(int value) {
    return (void *)PyInt_FromLong(value);
}

static chutney_creators creators = {
    creator_dealloc,    /* dealloc */       
    creator_null,       /* null */
    creator_bool,       /* bool */
    creator_int,        /* int */
    NULL, /* float */
    NULL, /* string */
    NULL, /* unicode */
    NULL, /* tuple */
    NULL, /* dict */
};

static PyObject *
chutney_loads(PyObject *self, PyObject *args)
{
    PyObject *obj;
    const char *data;
    int len;
    chutney_load_state state;

    if (!PyArg_ParseTuple(args, "S", &obj))
        return NULL;
    if (PyString_AsStringAndSize(obj, (char **)&data, &len) < 0)
        return NULL;
    if (chutney_load_init(&state, &creators) < 0) {
        PyErr_NoMemory();
        return NULL;
    }
    obj = NULL;
    switch (chutney_load(&state, &data, &len)) {
    case CHUTNEY_CONTINUE:
        PyErr_SetNone(PyExc_EOFError);
        break;
    case CHUTNEY_NOMEM:
        if (!PyErr_Occurred())
            PyErr_NoMemory();
        break;
    case CHUTNEY_OKAY:
        obj = (PyObject *)chutney_load_result(&state);
        if (obj) {
            Py_INCREF(obj);
            break;
        }
        /* fallthru */
    default:
	PyErr_SetString(UnpicklingError, "parse error");
        break;
    }
    chutney_load_dealloc(&state);
    return obj;
}

static int
cString_write(void *context, const char *s, long n)
{
    if (!s)
        return 0;

    if (PycStringIO->cwrite((PyObject *)context, (char *)s, (Py_ssize_t)n) != n)
        return -1;

    return (int)n;
}


static int
save(chutney_dump_state *self, PyObject *obj)
{
    PyTypeObject *type;
    int res = -1;

    if (self->depth++ > Py_GetRecursionLimit()){
        PyErr_SetString(PyExc_RuntimeError, "maximum recursion depth exceeded");
            goto finally;
    }
    if (obj == Py_None) {
        res = chutney_save_null(self);
        goto finally;
    }
    type = obj->ob_type;
    switch (type->tp_name[0]) {
    case 'b':
        if (obj == Py_False || obj == Py_True) {
            res = chutney_save_bool(self, obj == Py_True);
            goto finally;
        }
        break;

    case 'i':
        if (type == &PyInt_Type) {
            long value = PyInt_AS_LONG((PyIntObject *)obj);
            res = chutney_save_int(self, value);
            goto finally;
        }
        break;

    case 'f':
        if (type == &PyFloat_Type) {
            double value = PyFloat_AS_DOUBLE((PyFloatObject *)obj);
            res = chutney_save_float(self, value);
            goto finally;
        }
        break;

    case 's':
        if (type == &PyString_Type) {
            const char *value;
            int size = PyString_Size(obj);
            if (size >= 0 && size <= INT_MAX) {
                value = PyString_AS_STRING((PyStringObject *)obj);
                res = chutney_save_string(self, value, size);
            }
            goto finally;
        }
        break;

    case 'u':
        if (type == &PyUnicode_Type) {
            PyObject *value = NULL;
            int size;

            if (!(value = PyUnicode_AsUTF8String(obj)))
                goto unicode_finally;
            if ((size = PyString_Size(value)) < 0 || size > INT_MAX)
                goto unicode_finally;
            res = chutney_save_utf8(self, PyString_AS_STRING(value), size);
        unicode_finally:
            Py_XDECREF(value);
            goto finally;
        }
        break;

    case 't':
        if (type == &PyTuple_Type) {
            int i, len = PyTuple_Size(obj);
            if (len < 0)
                goto finally;
            if (chutney_save_mark(self) < 0)
                goto finally;
            for (i = 0; i < len; i++) {
                PyObject *element = PyTuple_GET_ITEM(obj, i);
                if (!element)
                    goto finally;
                if (save(self, element) < 0)
                    goto finally;
                
            }
            res = chutney_save_tuple(self);
            goto finally;
        }
        break;

    case 'l':
        if (type == &PyList_Type) {
            int i, len = PyList_Size(obj);
            if (len < 0)
                goto finally;
            if (chutney_save_mark(self) < 0)
                goto finally;
            for (i = 0; i < len; i++) {
                PyObject *element = PyList_GET_ITEM(obj, i);
                if (!element)
                    goto finally;
                if (save(self, element) < 0)
                    goto finally;
                
            }
            res = chutney_save_tuple(self);
            goto finally;
        }
        break;

    case 'd':
        if (type == &PyDict_Type) {
            PyObject *iter = NULL;
            int n, fail = 0;

            if (chutney_save_empty_dict(self) < 0)
                goto finally;
            iter = PyObject_CallMethod(obj, "iteritems", "()");
            if (iter == NULL)
		goto finally;
            do {
                PyObject *kv;
                for (n = 0; n < CHUTNEY_BATCHSIZE; ++n) {
                    if (!(kv = PyIter_Next(iter))) {
                        if (PyErr_Occurred())
                            goto dict_finally;
                        break;
                    }
                    if (n == 0)
                        if (chutney_save_mark(self) < 0) 
                            fail = 1;
                    if (!fail && save(self, PyTuple_GET_ITEM(kv, 0)) < 0)
                        fail = 1;
                    if (!fail && save(self, PyTuple_GET_ITEM(kv, 1)) < 0)
                        fail = 1;
                    Py_DECREF(kv);
                    if (fail)
                        goto dict_finally;
                }
                if (n > 0)
                    if (chutney_save_setitems(self) < 0)
                        goto dict_finally;
            } while (n == CHUTNEY_BATCHSIZE);
            res = 0;

        dict_finally:
            Py_XDECREF(iter);
            goto finally;
        }
        break;

    }
    PyErr_SetObject(UnpickleableError, obj);

finally:
    self->depth--;
    return res;
}    

static int
dump(chutney_dump_state *self, PyObject *obj)
{
    if (save(self, obj) < 0)
        return -1;

    if (chutney_save_stop(self) < 0)
        return -1;

    return 0;
}

static PyObject *
chutney_dumps(PyObject *self, PyObject *args)
{
    PyObject *obj, *file = NULL, *res = NULL;
    chutney_dump_state pickler;

    if (!(PyArg_ParseTuple(args, "O", &obj)))
        goto finally;

    if (!(file = PycStringIO->NewOutput(128)))
        goto finally;

    if (chutney_dump_init(&pickler, cString_write, (void *)file) < 0)
        goto finally;

    if (dump(&pickler, obj) < 0)
        goto finally;

    res = PycStringIO->cgetvalue(file);

finally:
    Py_XDECREF(file);

    return res;
}


static PyMethodDef chutney_methods[] = {
    {"loads",  chutney_loads, METH_VARARGS,
        "Load a chutney from the given string"},
    {"dumps",  chutney_dumps, METH_VARARGS,
        "Return a \"chutney\" of the given object"},
    {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(chutney_module_documentation,
"Simple and safe Python pickle module.");


PyMODINIT_FUNC
initchutney(void)
{
    PyObject *m;

    PycString_IMPORT;

    ChutneyError = PyErr_NewException("chutney.ChutneyError", NULL, NULL);
    if (!ChutneyError)
        return;

    UnpickleableError = PyErr_NewException("chutney.UnpickleableError", 
                                             ChutneyError, NULL);
    if (!UnpickleableError)
        return;

    UnpicklingError = PyErr_NewException("chutney.UnpicklingError", 
                                             ChutneyError, NULL);
    if (!UnpicklingError)
        return;

    m = Py_InitModule4("chutney", chutney_methods,
                       chutney_module_documentation,
		       (PyObject*)NULL, PYTHON_API_VERSION);
    if (!m)
        return;

    Py_INCREF(ChutneyError);
    PyModule_AddObject(m, "ChutneyError", ChutneyError);
    PyModule_AddObject(m, "UnpickleableError", UnpickleableError);
    PyModule_AddObject(m, "UnpicklingError", UnpicklingError);
}
