#include <iostream>
#include <vector>
#include <Python.h>
#include <unistd.h>
#include "adder.cc"

cxxrtl_design::top top;
uint32_t sim_time = 0;

struct signal_handler_t {
    int    id;
    const char * name;
    void * signal;
    int width;
    void (*setter) (void *signal, uint32_t v);
    uint32_t (*getter) (void *signal);
};

enum {OTHER_TRIGGER=0, TIMER_TRIGGER, EDGE_TRIGGER};

struct triggers_handler_t {
    uint32_t type;
    uint32_t data;
    bool (*func)(struct triggers_handler_t &);
};

std::vector<struct triggers_handler_t> triggers_handlers;

template <size_t Bits>
static void set_next_value(void *signal, uint32_t v) {
    ((wire<Bits>*)signal)->next = value<Bits>{v};
}

template <size_t Bits>
static uint32_t get_current_value(void *signal) {
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

static void delta() {
	do {
		top.eval();
	} while (top.commit());
    sim_time++;
}

static PyObject* get_by_id(PyObject *self, PyObject *args) {
    uint32_t id;
    PyArg_ParseTuple(args,"i", &id);
    return Py_BuildValue("i", sig_handler[id].getter(sig_handler[id].signal));
}

static PyObject* pydelta(PyObject *self, PyObject *args) {
    delta();
    Py_RETURN_NONE;
}

static PyObject* n_of_signals(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", sizeof(sig_handler)/sizeof(struct signal_handler_t));
}

static PyObject* get_sim_time(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", sim_time);
}

static PyObject* eval(PyObject *self, PyObject *args) {
    top.eval();
    Py_RETURN_NONE;
}

static bool timer_func(struct triggers_handler_t & t) {
    return --t.data == 0;
}

// static bool return_true(struct triggers_handler_t & t)
// {
//     return true;
// }

static PyObject* commit_trigger(PyObject *self, PyObject *args) {
    struct triggers_handler_t trigger;
    PyArg_ParseTuple(args,"ii", &trigger.type, &trigger.data);
    switch(trigger.type) {
        case TIMER_TRIGGER:
            trigger.func = timer_func;
            break;
        //case EDGE_TRIGGER:
        //    trigger.func = edge_func;
        //    break;
    }
    if (trigger.type == TIMER_TRIGGER) triggers_handlers.push_back(trigger);
    Py_RETURN_NONE;
}

static bool trigger_done() {
    bool done = false;
    bool result;
    for (auto it = triggers_handlers.begin(); it != triggers_handlers.end();)
    {
        result = it->func(*it);
        if (result) {
            triggers_handlers.erase(it);
            done = true;
        }
        else it++;
    }
    return done;
}

static PyObject* run_until_trigger(PyObject *self, PyObject * args) {
    do {
        delta();
        //sleep(1);
    } while (triggers_handlers.size() != 0 && not trigger_done());
    Py_RETURN_NONE;
}

static PyMethodDef simulator_methods[] = { 
    {"n_of_signals", n_of_signals, METH_NOARGS, "delta step"},
    {"get_signal_name", get_signal_name, METH_VARARGS, "set value by signal id"},
    {"run_until_trigger", run_until_trigger, METH_VARARGS, "set value by signal id"},
    {"commit_trigger", commit_trigger, METH_VARARGS, "set value by signal id"},
    {"delta", pydelta, METH_NOARGS, "delta step"},
    {"eval", eval, METH_NOARGS, "delta step"},
    {"get_sim_time", get_sim_time, METH_NOARGS, "delta step"},
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
