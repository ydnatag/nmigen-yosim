from nmigen import Fragment
from nmigen.back import rtlil, verilog
import importlib
import subprocess

class Signal():
    def __init__(self, simulation, signal_id):
        self.sim = simulation
        self.id = signal_id

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
            setattr(self, name, Signal(self.sim, i))

class Triggers:
    OTHER = 0
    TIMER = 1
    EDGE = 2

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
        self.main_coros = []
        self.child_coros = []

    def build(self):
        subprocess.run('make')

    def run(self, main_coros):
        loops = 0
        self.child_coros = []
        self.main_coros = list(main_coros)
        while True:
            for coro in self.main_coros:
                try:
                    t_type, aux = next(coro)
                    self.sim.commit_trigger(t_type, aux)
                except StopIteration:
                    return loops

            # Child coroutines can finish without finishing the simulation
            for coro in self.child_coros:
                try:
                    next(coro)
                except StopIteration:
                    self.child_coros.remove(coro)
            self.sim.run_until_trigger()
            loops += 1

    @property
    def sim_time(self):
        return self.sim.get_sim_time()

    def fork(self, coro):
        self.child_coros.append(coro)
        return coro

    def join(self, coro):
        while coro in self.main_coros or coro in self.child_coros:
            yield Triggers.OTHER, 0

    def __enter__(self):
        return self, self.dut

    def __exit__(self, exception_type, exception_val, trace):
        pass

def edge(s):
    prev = s.value
    while s.value == prev:
        yield

def falling_edge(s):
    prev = s.value
    while True:
        if s.value == 0 and prev == 1:
            return
        prev = s.value
        yield Triggers.EDGE, s.id

def rising_edge(s):
    prev = s.value
    while True:
        if s.value == 1 and prev == 0:
            # print('re', s.sim.get_sim_time())
            return
        prev = s.value
        yield Triggers.EDGE, s.id

def timer(time):
    yield Triggers.TIMER, time

def clock(clk, period=10):
    period_2 = int(period / 2)
    while True:
        yield from timer(period_2)
        clk.value = (clk.value + 1) % 2
        # print('clock', clk.sim.get_sim_time(), clk.value)
