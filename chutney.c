#include <Python.h>
#include <cStringIO.h>
#include "chutney.h"

#if (PY_VERSION_HEX < 0x02050000)
typedef int Py_ssize_t;
#endif

static PyObject *ChutneyError;

static PyObject *
chutney_loads(PyObject *self, PyObject *args)
{
    const char *data;

    if (!PyArg_ParseTuple(args, "s", &data))
        return NULL;
    Py_INCREF(Py_None);
    return Py_None;
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
    case 'i':
        if (type == &PyInt_Type) {
            res = chutney_save_int(self, PyInt_AS_LONG((PyIntObject *)obj));
            goto finally;
        }
        break;
    }
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

    m = Py_InitModule4("chutney", chutney_methods,
                       chutney_module_documentation,
		       (PyObject*)NULL, PYTHON_API_VERSION);
    if (!m)
        return;

    Py_INCREF(ChutneyError);
    PyModule_AddObject(m, "ChutneyError", ChutneyError);
}
