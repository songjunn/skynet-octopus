#include <Python.h>
#include "skynet.h"

struct python_client {
    char * module_name;
    PyObject * pModule;
    PyObject * pFuncCreate;
    PyObject * pFuncRelease;
    PyObject * pFuncCallback;
};

int python_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct python_client * instance = (struct python_client *)skynet_malloc(sizeof(struct python_client));
    instance->module_name = (char *)skynet_malloc(sizeof(char) * 64);
    sscanf(args, "%s", instance->module_name);

    //Py_SetPythonHome("c:/Python27");
    //Py_SetPythonHome("c:/Python27/Lib");

    Py_Initialize();

    // 检查初始化是否成功
    if (!Py_IsInitialized()) {  
        return 1;
    }

    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('./scripts')");

    ctx->hook = instance;

    instance->pModule = PyImport_ImportModule(instance->module_name);
    PyErr_Print();

    instance->pFuncCreate = PyObject_GetAttrString(instance->pModule, "create");
    PyErr_Print();

    instance->pFuncRelease = PyObject_GetAttrString(instance->pModule, "release");
    PyErr_Print();

    instance->pFuncCallback = PyObject_GetAttrString(instance->pModule, "handle");
    PyErr_Print();
    
    PyObject_CallFunction(instance->pFuncCreate, "ii", harbor, ctx->handle);
    PyErr_Print();
    
    return 0;
}

void python_release(struct skynet_service * ctx) {
    struct python_client * inst = (struct python_client *) ctx->hook;
    PyObject_CallFunction(inst->pFuncRelease, NULL, NULL);
    PyErr_Print();

    Py_DECREF(inst->pFuncCreate);
    Py_DECREF(inst->pFuncRelease);
    Py_DECREF(inst->pFuncCallback);
    Py_DECREF(inst->pModule);
    Py_Finalize();

    skynet_free(inst);
}

int python_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    struct python_client * inst = (struct python_client *) ctx->hook;

    PyObject * pArgs = PyTuple_New(5);
    PyTuple_SetItem(pArgs, 0, Py_BuildValue("i", ctx->handle));
    PyTuple_SetItem(pArgs, 1, Py_BuildValue("i", source));
    PyTuple_SetItem(pArgs, 2, Py_BuildValue("i", session));
    PyTuple_SetItem(pArgs, 3, Py_BuildValue("i", type));
    PyTuple_SetItem(pArgs, 4, Py_BuildValue("s#", (const char *) msg, sz));
    PyObject_CallObject(inst->pFuncCallback, pArgs);
    PyErr_Print();

    return 0;
}
