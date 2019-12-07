#include <iostream>
#include <vector>
#include <Python.h>
#include <unistd.h>
#include "adder.cc"

enum TRIGGERS {OTHER=0, TIMER, EDGE, R_EDGE, F_EDGE};

template <size_t Bits> static void set_next_value(void *signal, uint32_t v);
template <size_t Bits> static uint32_t get_current_value(void *signal);

std::vector<struct task_handler_t> main_tasks;
std::vector<struct task_handler_t> forked_tasks;

cxxrtl_design::p_top top;
uint32_t sim_time = 0;

struct signal_handler_t {
    int    id;
    const char * name;
    void * signal;
    int width;
    void (*setter) (void *signal, uint32_t v);
    uint32_t (*getter) (void *signal);
};

struct signal_handler_t sig_handler[] = {
    {.id = 0, .name = "rst", .width = 1,  .signal = (void*) &top.p_rst, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 1, .name = "clk", .width = 1,  .signal = (void*) &top.p_clk, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 2, .name = "r",   .width = 11, .signal = (void*) &top.p_r,   .setter = set_next_value<11>, .getter = get_current_value<11>},
    {.id = 3, .name = "b",   .width = 10, .signal = (void*) &top.p_b,   .setter = set_next_value<10>, .getter = get_current_value<10>},
    {.id = 4, .name = "a",   .width = 10, .signal = (void*) &top.p_a,   .setter = set_next_value<10>, .getter = get_current_value<10>},
};

bool timer_condition(void * data) {
    return (*(long *) data) == sim_time;
}


struct signal_status_t {
    uint32_t signal;
    uint32_t prev;
};

bool edge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    uint32_t current = sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (p->prev != current) {
        p->prev = current;
        return true;
    }
    return false;
}

bool redge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    uint32_t current = sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (p->prev == 0 &&  current == 1) {
        p->prev = current;
        return true;
    }
    p->prev = current;
    return false;
}

bool fedge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    uint32_t current = sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (p->prev == 1 &&  current == 0) {
        p->prev = current;
        return true;
    }
    p->prev = current;
    return false;
}

struct task_handler_t {
    PyObject *coro;
    bool (*condition)(void *);
    void * data;
    signal_status_t * p;
    void add_condition(long t, long d) {
        if (data) {free(data); data = NULL;}
        switch (t) {
            case TIMER:
                data = new long{sim_time + d};
                condition = timer_condition;
                break;
            case EDGE:
                p =  new signal_status_t;
                data = p;
                p->signal = d;
                p->prev = sig_handler[d].getter(sig_handler[d].signal);
                condition = edge_condition;
                break;
            case R_EDGE:
                p =  new signal_status_t;
                data = p;
                p->signal = d;
                p->prev = sig_handler[d].getter(sig_handler[d].signal);
                condition = redge_condition;
                break;
            case F_EDGE:
                p =  new signal_status_t;
                data = p;
                p->signal = d;
                p->prev = sig_handler[d].getter(sig_handler[d].signal);
                condition = fedge_condition;
                break;
        }
    }
};

template <size_t Bits>
static void set_next_value(void *signal, uint32_t v) {
    ((wire<Bits>*)signal)->next = value<Bits>{v};
}

template <size_t Bits>
static uint32_t get_current_value(void *signal) {
    return ((wire<Bits>*)signal)->curr.data[0];
}

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

static void step() {
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

static PyObject* pystep(PyObject *self, PyObject *args) {
    step();
    Py_RETURN_NONE;
}

static PyObject* n_of_signals(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", sizeof(sig_handler)/sizeof(struct signal_handler_t));
}

static PyObject* get_sim_time(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", sim_time);
}

static PyObject* add_task(PyObject *self, PyObject *args) {
    PyObject * gen;
    PyArg_ParseTuple(args,"O", &gen);
    main_tasks.push_back(task_handler_t{gen, NULL, NULL});
    Py_RETURN_NONE;
}

static PyObject* fork(PyObject *self, PyObject *args) {
    PyObject * coro;
    PyArg_ParseTuple(args,"O", &coro);

    auto ret = PyObject_CallMethod(coro, "__next__", "");
    if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
        PyErr_Clear();
        Py_RETURN_NONE;
    }
    if (PyErr_Occurred()) Py_RETURN_NONE;

    auto trigger = PyLong_AsLong(PyTuple_GET_ITEM(ret, 0));
    auto data = PyLong_AsLong(PyTuple_GET_ITEM(ret, 1));

    task_handler_t task = task_handler_t{coro, NULL, NULL};

    task.add_condition(trigger, data);
    forked_tasks.push_back(task);
    Py_RETURN_NONE;
}

static PyObject* scheduller(PyObject *self, PyObject *args) {
    PyObject *ret, *error;
    long trigger, data;
    
    while (true) {
        do {
            for (auto it=main_tasks.begin(); it != main_tasks.end();) {
                if (not it->condition or it->condition(it->data)) {
                    ret = PyObject_CallMethod(it->coro, "__next__", "");
                    if (PyErr_ExceptionMatches(PyExc_StopIteration)) Py_RETURN_NONE;
                    error = PyErr_Occurred();
                    if (error) Py_RETURN_NONE;
                    trigger = PyLong_AsLong(PyTuple_GET_ITEM(ret, 0));
                    data = PyLong_AsLong(PyTuple_GET_ITEM(ret, 1));
                    it->add_condition(trigger, data);
                }
                it++;
            }
            
            if (forked_tasks.size() != 0) {
                for (auto it=forked_tasks.begin(); it != forked_tasks.end();) {
                    if (not it->condition or it->condition(it->data)) {
                        ret = PyObject_CallMethod(it->coro, "__next__", "");
                        if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                            if (it->data) {free(it->data); it->data = NULL;}
                            forked_tasks.erase(it);
                            PyErr_Clear();
                        }
                        error = PyErr_Occurred();
                        if (error) Py_RETURN_NONE;
                        trigger = PyLong_AsLong(PyTuple_GET_ITEM(ret, 0));
                        data = PyLong_AsLong(PyTuple_GET_ITEM(ret, 1));
                        it->add_condition(trigger, data);
                        it++;
                    }
                    else it++;
                }
            }

            top.eval();
        } while(top.commit());
        sim_time++;
        if (PyErr_CheckSignals()) Py_RETURN_NONE;
    }
    std::cout << "leaving scheduller" << std::endl;
    Py_RETURN_NONE;
}

static PyMethodDef simulator_methods[] = { 
    // Signal access
    {"n_of_signals", n_of_signals, METH_NOARGS, "delta step"},
    {"get_signal_name", get_signal_name, METH_VARARGS, "set value by signal id"},
    {"set_by_id", set_by_id, METH_VARARGS, "set value by signal id"},
    {"get_by_id", get_by_id, METH_VARARGS, "get value by signal id"},

    // Time step
    {"step", pystep, METH_NOARGS, "time step"},

    // Scheduller
    {"fork", fork, METH_VARARGS, "add task to scheduller"},
    {"add_task", add_task, METH_VARARGS, "add task to scheduller"},
    {"scheduller", scheduller, METH_NOARGS, "add task to scheduller"},

    // Utils
    {"get_sim_time", get_sim_time, METH_NOARGS, "delta step"},

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
