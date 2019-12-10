from nmigen import Fragment
from nmigen.back import rtlil, verilog
import importlib
import subprocess
import sys

class Signal():
    def __init__(self, simulation, signal_id, name):
        self.sim = simulation
        self.id = signal_id
        self.name = name

    @property
    def value(self):
        return self.sim.get_by_id(self.id)

    @value.setter
    def value(self, value):
        return self.sim.set_by_id(self.id, value)

    def __str__(self):
        return str(self.value)

class Dut:
    def __init__(self, simulation):
        self.sim = simulation
        self.sim.n_of_signals()
        for i in range(self.sim.n_of_signals()):
            name = self.sim.get_signal_name(i)
            setattr(self, name, Signal(self.sim, i, name))

class TRIGGERS:
    OTHER = 0
    TIMER = 1
    EDGE = 2
    R_EDGE = 3
    F_EDGE = 4

class Simulator:
    def __init__(self, design, platform=None, ports=None):
        fragment = Fragment.get(design, None)
        output = rtlil.convert(fragment, name='top', ports=ports)

        with open('adder.il', 'w') as f:
            f.write(output)
        self.build()

        output = verilog.convert(fragment, name='top', ports=ports)
        with open('top.v', 'w') as f:
            f.write(output)

        import simulation as sim
        self.sim = sim
        self.dut = Dut(sim)

    def build(self):
        subprocess.run('make').check_returncode()

    def run(self, coros):
        for coro in coros:
            self.sim.add_task(coro)
        try:
            try:
                self.sim.scheduller()
            except Exception as e:
                raise e.__cause__ from None
        except StopIteration:
            pass

    @property
    def sim_time(self):
        return self.sim.get_sim_time()

    def fork(self, coro):
        self.sim.fork(coro)
        return coro

    def join(self, coro):
        while coro in self.main_coros or coro in self.child_coros:
            yield Triggers.OTHER, 0

    def __enter__(self):
        return self, self.dut

    def __exit__(self, exception_type, exception_val, trace):
        pass

def edge(s):
    return TRIGGERS.EDGE, s.id

def falling_edge(s):
    return TRIGGERS.F_EDGE, s.id

def rising_edge(s):
    return TRIGGERS.R_EDGE, s.id

def timer(time):
    return TRIGGERS.TIMER, time

def clock(clk, period=10):
    period_2 = int(period / 2)
    while True:
        yield timer(period_2)
        clk.value = (clk.value + 1) % 2
