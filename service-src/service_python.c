#include <Python.h>
#include "skynet.h"

struct python_client {
    PyObject * pModule;
    PyObject * pFuncCreate;
    PyObject * pFuncRelease;
    PyObject * pFuncCallback;
};

int python_create(struct skynet_service * ctx, int harbor, const char * args) {
    char name[64], path[256], cmd[512];
    sscanf(args, "%[^','],%s", path, name);
    sprintf(cmd, "sys.path.append('%s')", path);

    Py_Initialize();

    // 检查初始化是否成功
    if (!Py_IsInitialized()) {  
        return 1;
    }

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(cmd);

    struct python_client * instance = skynet_malloc(sizeof(struct python_client));
    ctx->hook = instance;

    instance->pModule = PyImport_ImportModule(name);
    PyErr_Print();

    instance->pFuncCreate = PyObject_GetAttrString(instance->pModule, "create");
    PyErr_Print();

    instance->pFuncRelease = PyObject_GetAttrString(instance->pModule, "release");
    PyErr_Print();

    instance->pFuncCallback = PyObject_GetAttrString(instance->pModule, "handle");
    PyErr_Print();
    
    PyObject_CallFunction(instance->pFuncCreate, "iis#", harbor, ctx->handle, args, strlen(args));
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
    //存在多个python服务时，会导致第二个及后续的服务执行python脚本失败
    //Py_Finalize();

    skynet_free(inst);
}

int python_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    struct python_client * inst = (struct python_client *) ctx->hook;

    PyObject * pArgs = PyTuple_New(5);
    PyTuple_SetItem(pArgs, 0, Py_BuildValue("i", ctx->handle));
    PyTuple_SetItem(pArgs, 1, Py_BuildValue("i", source));
    PyTuple_SetItem(pArgs, 2, Py_BuildValue("i", session));
    PyTuple_SetItem(pArgs, 3, Py_BuildValue("i", type));
    PyTuple_SetItem(pArgs, 4, Py_BuildValue("y#", (const char *) msg, sz));
    PyObject_CallObject(inst->pFuncCallback, pArgs);
    PyErr_Print();

    return 0;
}
