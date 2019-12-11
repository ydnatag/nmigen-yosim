#include <iostream>
#include <vector>
#include <Python.h>
#include <unistd.h>
#include <stdio.h>

enum TRIGGERS {OTHER=0, TIMER, EDGE, R_EDGE, F_EDGE};

template <size_t Bits> static void set_next_value(void *signal, PyObject *v);
template <size_t Bits> static PyObject * get_current_value(void *signal);

std::vector<struct task_handler_t> main_tasks;
std::vector<struct task_handler_t> forked_tasks;

//#define DEBUG
#ifdef DEBUG
    #define DEBUG_PRINT(...) do{ fprintf( stderr, __VA_ARGS__ ); } while( false )
#else
    #define DEBUG_PRINT(...) do{} while( false )
#endif

cxxrtl_design::p_top top;
uint32_t sim_time = 0;

struct signal_handler_t {
    int    id;
    const char * name;
    void * signal;
    int width;
    void (*setter) (void *signal, PyObject *v);
    PyObject * (*getter) (void *signal);
};

struct signal_status_t {
    uint32_t signal;
    PyLongObject * prev;
};

struct signal_handler_t sig_handler[] = {
    {.id = 0, .name = "rst", .width = 1,  .signal = (void*) &top.p_rst, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 1, .name = "clk", .width = 1,  .signal = (void*) &top.p_clk, .setter = set_next_value<1>,  .getter = get_current_value<1> },
    {.id = 2, .name = "r",   .width = 65, .signal = (void*) &top.p_r,   .setter = set_next_value<65>, .getter = get_current_value<65>},
    {.id = 3, .name = "b",   .width = 64, .signal = (void*) &top.p_b,   .setter = set_next_value<64>, .getter = get_current_value<64>},
    {.id = 4, .name = "a",   .width = 64, .signal = (void*) &top.p_a,   .setter = set_next_value<64>, .getter = get_current_value<64>},
};

static bool timer_condition(void * data) {
    if ((*(long *) data) == sim_time) {
        return true;
    }
    return false;
}

static bool PyLongEquals(PyLongObject *a, PyLongObject *b){
    auto size_a = Py_SIZE(a);
    auto size_b = Py_SIZE(b);

    if (size_a != size_b) return false;

    if(size_a == 0) return true;
    for (unsigned int i=0; i<size_a; i++) {
        if (a->ob_digit[i] != b->ob_digit[i]) return false;
    }
    return true;
}

static bool edge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    PyLongObject * current = (PyLongObject *) sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (not PyLongEquals(p->prev, current)) {
        p->prev = current;
        return true;
    }
    return false;
}

static bool redge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    PyLongObject * current = (PyLongObject *) sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (Py_SIZE(p->prev) == 0 &&  Py_SIZE(current)) {
        p->prev = current;
        DEBUG_PRINT("@%d ps: RISING EDGE\n", sim_time);
        return true;
    }
    p->prev = current;
    return false;
}

static bool fedge_condition(void * data) {
    signal_status_t * p = (signal_status_t*) data;
    PyLongObject * current = (PyLongObject *) sig_handler[p->signal].getter(sig_handler[p->signal].signal);
    if (Py_SIZE(p->prev) &&  Py_SIZE(current) == 0) {
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
                p->prev = (PyLongObject*) sig_handler[d].getter(sig_handler[d].signal);
                condition = edge_condition;
                break;
            case R_EDGE:
                p =  new signal_status_t;
                data = p;
                p->signal = d;
                p->prev =  (PyLongObject*) sig_handler[d].getter(sig_handler[d].signal);
                condition = redge_condition;
                break;
            case F_EDGE:
                p =  new signal_status_t;
                data = p;
                p->signal = d;
                p->prev =  (PyLongObject*) sig_handler[d].getter(sig_handler[d].signal);
                condition = fedge_condition;
                break;
        }
    }
};

template <size_t Bits>
static void set_next_value(void *signal, PyObject *v) {
    PyLongObject * l = (PyLongObject*) v;
    value<Bits> *p = &((wire<Bits>*) signal)->next;

    const uint32_t chunks = (Bits + 32 - 1) / 32;
    long ob_size = Py_SIZE(l);
    uint64_t data = 0;
    int b = 0;

    unsigned int i = 0, j = 0;
    for (unsigned int i = 0; i < chunks; i++) p->data[i] = 0;
    if (ob_size != 0) {
        for (unsigned int i = 0; i < ob_size; i++) {
            data |= (((uint64_t) (l->ob_digit[i] & 0x3fffffff)) << b);
            b += 30;
            while(b >= 32) {
                p->data[j] = data & 0xffffffff;
                data = data >> 32;
                b -= 32;
                j++;
            }
        }
        if (b != 0) p->data[j] = data & 0xffffffff;;
    }
    else for (unsigned int i = 0; i < p->chunks; i++) p->data[i] = 0;
}

template <size_t Bits>
static PyObject * get_current_value(void *signal) {
    value<Bits> *p = &((wire<Bits>*) signal)->curr;
    uint64_t data = 0;
    uint32_t b = 0;
    const uint32_t chunks = (Bits + 32 - 1) / 32;
    const uint32_t max_ob_size = (Bits + 30 - 1) / 30 + 1;
    uint32_t  ob_size = 0;
    unsigned int j = 0;
    uint32_t ob_digit[max_ob_size] = {};

    for (unsigned int i = 0; i < chunks; i++) {
        if (signal != &top.p_clk ) DEBUG_PRINT("p->data[%d] = %u\n", i, p->data[i]); 
        data |= ((uint64_t) p->data[i]) << b;
        b += 32;
        while(b >= 30) {
            ob_digit[j] = data & 0x3fffffff;
            data = data >> 30;
            b -= 30;
            j++;
        }
    }
    if (b != 0) ob_digit[j] = data;
    ob_size = max_ob_size -1;
    while(ob_size > 0 && ob_digit[ob_size] == 0) ob_size--;
    if (ob_size == 0 && ob_digit[0] == 0) return PyLong_FromLong(0L);


    PyLongObject * l = _PyLong_New(ob_size+ 1);
    for (unsigned int i = 0; i <= ob_size; i++) l->ob_digit[i] = ob_digit[i];
    return (PyObject *) l;
}

static PyObject* get_signal_name(PyObject *self, PyObject *args) {
    uint32_t id;
    PyArg_ParseTuple(args,"i", &id);
    return Py_BuildValue("s", sig_handler[id].name);
}

static PyObject* get_signal_width(PyObject *self, PyObject *args) {
    uint32_t id;
    PyArg_ParseTuple(args,"i", &id);
    return Py_BuildValue("s", sig_handler[id].width);
}

static PyObject* set_by_id(PyObject *self, PyObject *args) {
    uint32_t id;
    PyObject * v;
    PyArg_ParseTuple(args,"iO", &id, &v);
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
    DEBUG_PRINT("get_by_id: %s\n", sig_handler[id].name); 
    return sig_handler[id].getter(sig_handler[id].signal);
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

static struct PyModuleDef simulation_definition = { 
    PyModuleDef_HEAD_INIT,
    "simulation",
    "yosys simulation in python",
    -1, 
    simulator_methods
};

PyMODINIT_FUNC PyInit_simulation(void) {
    Py_Initialize();
    return PyModule_Create(&simulation_definition);
}
