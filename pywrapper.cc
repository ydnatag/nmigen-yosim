#include <iostream>
#include <Python.h>
#include "adder.cc"

cxxrtl_design::top top;

struct signal_handler_t {
    int    id;
    const char * name;
    void * signal;
    int width;
    void (*setter) (void *signal, uint32_t v);
    uint32_t (*getter) (void *signal);
};

template <size_t Bits>
void set_next_value(void *signal, uint32_t v) {
    ((wire<Bits>*)signal)->next = value<Bits>{v};
}

template <size_t Bits>
uint32_t get_current_value(void *signal) {
    return ((wire<Bits>*)signal)->curr.data[0];
}

struct signal_handler_t sig_handler[] = {
    {.id = 0, .name = "rst", .width = 1,  .signal = (void*) &top.rst, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 1, .name = "clk", .width = 1,  .signal = (void*) &top.clk, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 2, .name = "r",   .width = 11, .signal = (void*) &top.r,   .setter = set_next_value<11>, .getter = get_current_value<11>},
    {.id = 3, .name = "b",   .width = 10, .signal = (void*) &top.b,   .setter = set_next_value<10>, .getter = get_current_value<10>},
    {.id = 4, .name = "a",   .width = 10, .signal = (void*) &top.a,   .setter = set_next_value<10>, .getter = get_current_value<10>},
};

static PyObject* get_signal_name(PyObject *self, PyObject *args) {
    uint32_t id;
    PyArg_ParseTuple(args,"i", &id);
    return Py_BuildValue("s", sig_handler[id].name);
}

static PyObject* set_by_id(PyObject *self, PyObject *args) {
    uint32_t id;
    uint32_t v;
    PyArg_ParseTuple(args,"ii", &id, &v);
    sig_handler[id].setter(sig_handler[id].signal, v);
    Py_RETURN_NONE;
}

static PyObject* get_by_id(PyObject *self, PyObject *args) {
    uint32_t id;
    PyArg_ParseTuple(args,"i", &id);
    return Py_BuildValue("i", sig_handler[id].getter(sig_handler[id].signal));
}

static PyObject* delta(PyObject *self, PyObject *args) {
	do {
		top.eval();
	} while (top.commit());
    Py_RETURN_NONE;
}

static PyObject* n_of_signals(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", sizeof(sig_handler)/sizeof(struct signal_handler_t));
}

static PyObject* eval(PyObject *self, PyObject *args) {
    top.eval();
    Py_RETURN_NONE;
}

static PyMethodDef simulator_methods[] = { 
    {"n_of_signals", n_of_signals, METH_NOARGS, "delta step"},
    {"get_signal_name", get_signal_name, METH_VARARGS, "set value by signal id"},
    {"delta", delta, METH_NOARGS, "delta step"},
    {"eval", eval, METH_NOARGS, "delta step"},
    {"set_by_id", set_by_id, METH_VARARGS, "set value by signal id"},
    {"get_by_id", get_by_id, METH_VARARGS, "get value by signal id"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef hello_definition = { 
    PyModuleDef_HEAD_INIT,
    "simulation",
    "yosys simulation in python",
    -1, 
    simulator_methods
};

PyMODINIT_FUNC PyInit_simulation(void) {
    Py_Initialize();
    return PyModule_Create(&hello_definition);
}
