#include <Python.h>

#include "sproto.h"

static int
_encode(void *ud, const char *tagname, int type, int index, struct sproto_type *st, void *value, int length) {
    PyObject *table = ud;
    PyObject *pyval;
    if (!PyDict_Check(table)) {
        PyErr_SetString(PyExc_TypeError, "type of dict is expected");
        return 0;
    }
    pyval = PyDict_GetItemString(table, tagname);
    if (pyval && index) {       /* is array? */
        if (!PyList_Check(pyval)) {
            PyErr_SetString(PyExc_TypeError, "type of list is expected");
            return 0;
        }
        pyval = PyList_GetItem(pyval, index);
    }
    if (!pyval)
        return 0;
    switch (type) {
    case SPROTO_TINTEGER: {
        if (!PyInt_Check(pyval)) {
            return 0;
        }
        long v = PyInt_As_LONG(pyval);
        long vh = v >> 31;
		if (vh == 0 || vh == -1) {
			*(uint32_t *)value = (uint32_t)v;
			return 4;
		}
		else {
			*(uint64_t *)value = (uint64_t)v;
			return 8;
		}
    }
    case SPROTO_TBOOLEAN {
        if (!PyBool_Check(pyval)) {
            return 0;
        }
        *(int *)value = (pyval == Py_True);
        return 4;
    }
	case SPROTO_TSTRING: {
        if (!PyString_Check(pyval)) {
            return 0;
        }
        const char* str;
        size_t sz = 0;
        PyString_AsStringAndSize(pyval, &str, &sz);
        if (sz > length)
			return -1;
		memcpy(value, str, sz);
        return sz;
	}
    case SPROTO_TSTRUCT: {
        return sproto_encode(st, value, length, _encode, pyval);
    }
    default:
        /* error */
    }
}

static PyObject*
pysproto_new(PyObject *self, PyObject *args) {
    struct sproto * sp;
    const char *buf;
    size_t sz;
    PyArg_ParseTuple(args, "sI", &buf, &sz);
    sp = sproto_create(buf, sz);
    if (sp) 
        return PyCapsule_New(sp, "pysproto", NULL);
    return Py_None;
}

static PyObject*
pysproto_encode(PyObject *self, PyObject *args) {
    PyObject *capsule, *table;
    struct sproto_type *st;
    void *buffer;
    int sz;

    PyArg_ParseTuple(args, "OO", &capsule, &table);
    st = PyCapsule_GetPointer(capsule, "pysproto");

    buffer = initbuffer(buffer, &sz);

    for (;;) {
        int r = sproto_encode(st, buffer, sz, _encode, table);
        if (r<0) {
            buffer = expand_buffer(buffer, sz, sz*2);
            sz *= 2;
        } else {
            return Py_BuildValue("s#", buffer, r);
        }
    }
}

static PyObject*
pysproto_decode(PyObject *self, PyObject *args) {
    struct sproto_type *st;
    const char* buffer;
    size_t sz;
    int r;
    PyObject *dict;

    PyArg_ParseTuple(args, "Os#", &st, &buffer, &sz);

    dict = PyDict_New();
    r = sproto_decode(st, buffer, (int)sz, _decode, dict);
    if (r < 0) {
        /* report error */
        return Py_None;
    }
    return dict;
}

static PyObject*
pysproto_test(PyObject *self, PyObject *args) {
    PyObject *o;
    PyArg_ParseTuple(args, "O", &o);
    const char* str = PyCapsule_GetPointer(o, NULL);
    return Py_BuildValue("s", str);
}

static PyMethodDef SprotoMethods[] = {
    {"new", pysproto_new, METH_VARARGS, NULL},
    {"delete", pysproto_del, METH_VARARGS, NULL},
    {"dump", pysproto_dump, METH_VARARGS, NULL},

    {"protocal", pysproto_new, METH_VARARGS, NULL},

    {"encode", pysproto_encode, METH_VARARGS, NULL},
    {"decode", pysproto_decode, METH_VARARGS, NULL},

    {"pack", pysproto_pack, METH_VARARGS, NULL},
    {"unpack", pysproto_unpack, METH_VARARGS, NULL},

    {NULL, NULL, 0, NULL},
};

PyMODINIT_FUNC
initsproto(void) {
    Py_InitModule("pysproto", SprotoMethods);
}
