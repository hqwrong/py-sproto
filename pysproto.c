#include <stdarg.h>
#include <stdio.h>

#include <Python.h>

#include "clib/sproto.h"

#define ENCODE_BUFFERSIZE 2050

static PyObject *SprotoError;

/* ------------------------------ buf api ------------------ */
struct spbuf {
    void *buf;
    size_t sz;
};

struct spbuf*
newbuf() {
    struct spbuf *spbuf = PyMem_Malloc(sizeof(spbuf));
    spbuf->buf = NULL;
    spbuf->sz = 0;

    return spbuf;
}

void
expandbuf(struct spbuf *spbuf, size_t n) {
    if (n > spbuf->sz) {
        spbuf->buf = PyMem_Malloc(n);
        spbuf->sz = n;
    }
}

void
delbuf(struct spbuf *spbuf) {
    if (spbuf->buf)
        PyMem_Free(spbuf->buf);
    PyMem_Free(spbuf);
}
/* ----------------------------------------- end buf api ----------------- */

int
_error(const char* tagname, int index, const char* msg) {
    PyErr_SetObject(SprotoError, PyString_FromFormat("%s @ tag[%s] index[%d]", msg, tagname, index));
    return -1;
}

int
_strerr(const char* msg) {
    PyErr_SetString(SprotoError, msg);
    return -1;
}

void
_free_sp(PyObject *spcap) {
    struct spbuf *spbuf = PyCapsule_GetContext(spcap);
    struct sproto *sp = PyCapsule_GetPointer(spcap, "pysproto");
    delbuf(spbuf);
    sproto_release(sp);
}

static int
_encode(const struct sproto_arg *args) {
    const char *tagname = args->tagname;
    PyObject *dict = args->ud;
    int type = args->type;
    int index = args->index;
    int length = args->length;
    struct sproto_type *st = args->subtype;
    void *value = args->value;

    PyObject *pyval = NULL;
    if (!PyDict_Check(dict))
        return _error(tagname, index, "need dict");
    
    pyval = PyDict_GetItemString(dict, tagname);
    if (pyval && index) {       /* is array? */
        if (!PyList_Check(pyval)) 
            return _error(tagname, index, "need list");
        if (PyList_Size(pyval) < index)
            pyval = NULL;
        else
            pyval = PyList_GetItem(pyval, index-1);
    }
    if (!pyval)
        return 0;
    switch (type) {
    case SPROTO_TINTEGER: {
        if (!PyInt_Check(pyval)) 
            return _error(tagname, index, "need integer");
        long v = PyInt_AsLong(pyval);
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
    case SPROTO_TBOOLEAN: {
        if (!PyBool_Check(pyval)) 
            return _error(tagname, index, "need boolean");
        *(int *)value = (pyval == Py_True);
        return 4;
    }
	case SPROTO_TSTRING: {
        if (!PyString_Check(pyval)) 
            return _error(tagname, index, "need string");
        char* str;
        size_t sz = 0;
        PyString_AsStringAndSize(pyval, &str, &sz);
        if (sz > length)
			return -1;
		memcpy(value, str, sz);
        return sz + 1;   // The length of empty string is 1.
	}
    case SPROTO_TSTRUCT: {
        return sproto_encode(st, value, length, _encode, pyval);
    }
    default:
        return _error(tagname, index, "unexpected type");
    }
}

static int
_decode(const struct sproto_arg *args) {
    const char *tagname = args->tagname;
    PyObject *table = args->ud;
    int type = args->type;
    int index = args->index;
    int length = args->length;
    struct sproto_type *st = args->subtype;
    void *value = args->value;

    PyObject *list = PyDict_GetItemString(table, tagname);
    PyObject *pyval;

    if (index > 0 && !list) {
        list = PyList_New(0);
        PyDict_SetItemString(table, tagname, list);
    }

    switch (type) {
    case SPROTO_TINTEGER:
        pyval = PyInt_FromLong(*(long*)value); break;
    case SPROTO_TBOOLEAN:
        pyval = (*(long*)value) == 1 ? Py_True : Py_False; break;
    case SPROTO_TSTRING:
        pyval = PyString_FromStringAndSize(value, length); break;
    case SPROTO_TSTRUCT: {
        pyval = PyDict_New();
        int r = sproto_decode(st, value, length, _decode, pyval);
        if (r < 0) 
            return _error(tagname, index, "decode failed");
    }
    }

    if (index)
        PyList_Append(list, pyval);
    else
        PyDict_SetItemString(table, tagname, pyval);

    return 0;
}

static PyObject*
py_new(PyObject *self, PyObject *args) {
    struct sproto * sp;
    const char *buf;
    int sz = 0;
    PyArg_ParseTuple(args, "s#", &buf, &sz);
    sp = sproto_create(buf, sz);
    if (!sp)
        return NULL;

    PyObject *spcap = PyCapsule_New(sp, "pysproto", _free_sp);
    PyCapsule_SetContext(spcap, newbuf());
    return spcap;
}

static PyObject*
py_encode(PyObject *self, PyObject *args) {
    PyObject *stcap, *spcap, *dict;
    struct sproto_type *st;
    struct spbuf *spbuf;

    PyArg_ParseTuple(args, "OOO", &spcap, &stcap, &dict);
    st = PyCapsule_GetPointer(stcap, "sproto_type");
    spbuf = PyCapsule_GetContext(spcap);

    for (;;) {
        int r = sproto_encode(st, spbuf->buf, spbuf->sz, _encode, dict);
        if (PyErr_Occurred())           /* error occurred? */
            return NULL;
        if (r<0) 
            expandbuf(spbuf, spbuf->sz ? spbuf->sz*2 : ENCODE_BUFFERSIZE);
        else 
            return Py_BuildValue("s#", spbuf->buf, r);
    }
}

static PyObject*
py_decode(PyObject *self, PyObject *args) {
    PyObject *stcap, *spcap, *dict;
    struct sproto_type *st;
    const char* buffer;
    int sz;
    int r;

    PyArg_ParseTuple(args, "OOs#", &spcap, &stcap, &buffer, &sz);
    st = PyCapsule_GetPointer(stcap, "sproto_type");

    dict = PyDict_New();
    r = sproto_decode(st, buffer, (int)sz, _decode, dict);
    if (r < 0) {
        _error(".toplevel", 0, "decode failed");
        return NULL;
    }
    if (PyErr_Occurred())
        return NULL;
    return Py_BuildValue("(Oi)", dict, r);
}

static PyObject*
py_query(PyObject *self, PyObject *args) {
    char * typename;
    PyObject *spcap;
    struct sproto *sp;
    struct sproto_type *st;

    PyArg_ParseTuple(args, "Os", &spcap, &typename);
    sp = PyCapsule_GetPointer(spcap, "pysproto");
    st = sproto_type(sp, typename);
    if (!st) {
        _strerr(typename);
        return NULL;
    }

    return PyCapsule_New(st, "sproto_type", NULL);
}

static PyObject*
py_protocal(PyObject *self, PyObject *args) {
    struct sproto *sp;
    const char *protoname;
    PyObject *spcap, *handle;
    int tag;
	struct sproto_type *request, *response;
    PyObject *req = Py_None;
    PyObject *resp = Py_None;

    PyArg_ParseTuple(args, "OO", &spcap, &handle);
    sp = PyCapsule_GetPointer(spcap, "pysproto");
    if (PyString_Check(handle)) {
        protoname = PyString_AsString(handle);
        tag = sproto_prototag(sp, protoname);
        if (tag < 0) {
            _strerr(protoname);
            return NULL;
        }
    } else if (PyInt_Check(handle)) {
        tag = PyInt_AsLong(handle);
        protoname = sproto_protoname(sp, tag);
        if (protoname == NULL) {
            _strerr("invalid proto tag");
            return NULL;
        }
    }
    else {
        _strerr("unexpected protoname");
        return NULL;
    }

	request = sproto_protoquery(sp, tag, SPROTO_REQUEST);
	response = sproto_protoquery(sp, tag, SPROTO_RESPONSE);
    if (request)
        req = PyCapsule_New(request, "sproto_type", NULL);
    if (response)
        resp = PyCapsule_New(response, "sproto_type", NULL);

    if (PyString_Check(handle)) 
        return Py_BuildValue("(OOi)", req, resp, tag);
    else
        return Py_BuildValue("(OOs)", req, resp, protoname);
}

static PyObject*
py_pack(PyObject *self, PyObject *args) {
    PyObject * spcap;
    const void* buffer;
    struct spbuf *spbuf;
    int sz = 0;
    
    PyArg_ParseTuple(args, "Os#", &spcap, &buffer, &sz);
    spbuf = PyCapsule_GetContext(spcap);
	// the worst-case space overhead of packing is 2 bytes per 2 KiB of input (256 words = 2KiB).
	size_t maxsz = (sz + 2047) / 2048 * 2 + sz;
    if (spbuf->sz < maxsz)
        expandbuf(spbuf, maxsz);

	int bytes = sproto_pack(buffer, sz, spbuf->buf, maxsz);
	if (bytes > maxsz)
        return NULL;
    return Py_BuildValue("s#", spbuf->buf, bytes);
}

static PyObject*
py_unpack(PyObject *self, PyObject *args) {
    const void* buffer;
    PyObject *spcap;
    struct spbuf *spbuf;
    int sz = 0;
    
    PyArg_ParseTuple(args, "Os#", &spcap, &buffer, &sz);
    spbuf = PyCapsule_GetContext(spcap);

    int r = sproto_unpack(buffer, sz, spbuf->buf, spbuf->sz);
    if (r <0) {
        _strerr("invalid unpack stream");
        return NULL;
    }
    if (r > spbuf->sz)
        expandbuf(spbuf, r);
    r = sproto_unpack(buffer, sz, spbuf->buf, spbuf->sz);
    if (r<0) {
        _strerr("invalid unpack stream");
        return NULL;
    }
    return Py_BuildValue("s#", spbuf->buf, r);
}

static PyMethodDef SprotoMethods[] = {
    {"new", py_new, METH_VARARGS, NULL},

    {"querytype", py_query, METH_VARARGS, NULL},
    {"protocal", py_protocal, METH_VARARGS, NULL},

    {"encode", py_encode, METH_VARARGS, NULL},
    {"decode", py_decode, METH_VARARGS, NULL},

    {"pack", py_pack, METH_VARARGS, NULL},
    {"unpack", py_unpack, METH_VARARGS, NULL},

    {NULL, NULL, 0, NULL},
};

PyMODINIT_FUNC
initpysproto(void) {
    PyObject *m = Py_InitModule("pysproto", SprotoMethods);
    SprotoError = PyErr_NewException("sproto.error", NULL, NULL);
    Py_INCREF(SprotoError);
    PyModule_AddObject(m, "error", SprotoError);
}
