#include <Python.h>
#include "skynet.h"

static PyObject* py_skynet_sendname(PyObject *self, PyObject *args)
{
    char *name, *msg;
    int type, size;
    uint32_t source;
    uint32_t session;
 
    if (!PyArg_ParseTuple(args, "siiisi", &name, &source, &session, &type, &msg, &size))
        return NULL;

    skynet_sendname(name, source, session, type, msg, size);

    Py_RETURN_NONE;
}

static PyObject* py_skynet_sendhandle(PyObject *self, PyObject *args)
{
    char *msg;
    int type, size;
    uint32_t handle;
    uint32_t source;
    uint32_t session;
 
    if (!PyArg_ParseTuple(args, "iiiisi", &handle, &source, &session, &type, &msg, &size))
        return NULL;

    skynet_sendhandle(handle, source, session, type, msg, size);

    Py_RETURN_NONE;
}

static PyObject* py_skynet_logger_debug(PyObject *self, PyObject *args)
{
    char *msg;
    uint32_t source;
 
    if (!PyArg_ParseTuple(args, "is", &source, &msg))
        return NULL;

    skynet_logger_print(source, LOGGER_DEBUG, msg);

    Py_RETURN_NONE;
}

static PyMethodDef PyModuleMethods[] = {
    {"skynet_sendname", py_skynet_sendname, METH_VARARGS, NULL},
    {"skynet_sendhandle", py_skynet_sendhandle, METH_VARARGS, NULL},
    {"skynet_logger_debug", py_skynet_logger_debug, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};
 
PyMODINIT_FUNC initpySkynet(void) {
    (void) Py_InitModule("pySkynet", PyModuleMethods);
}
