from nmigen import *
from nmigen.back import rtlil
import importlib
import subprocess

class SimSignal():
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
            setattr(self, name, SimSignal(self.sim, i))

class Simulator:
    def __init__(self, design, platform=None, ports=None):
        fragment = Fragment.get(design, None)
        output = rtlil.convert(fragment, name='top', ports=ports)

        with open('adder.il', 'w') as f:
            f.write(output)
        self.build()

        import simulation as sim
        self.sim = sim
        self.dut = Dut(sim)

    def build(self):
        subprocess.run('make')

    def run(self, coros):
        loops = 0
        try:
            while True:
                for coro in coros:
                    next(coro)
                self.sim.delta()
                loops += 1
        except StopIteration:
            return loops

    def __enter__(self):
        return self, self.dut

    def __exit__(self, exception_type, exception_val, trace):
        pass

def rising_edge(s):
    prev = s.value
    while True:
        if s.value == 1 and prev == 0:
            return
        prev = s.value
        yield

def clock(s):
    while True:
        s.value = (s.value + 1) % 2
        yield
