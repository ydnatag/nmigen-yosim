from nmigen import *
from nmigen.cli import main
from nmigen.back import rtlil
from nmigen.hdl.rec import Direction
import subprocess
import time

class Adder(Elaboratable):
    def __init__(self, width, domain='comb', interface=None):
        self.width = width
        self.interface = interface
        self.a = Signal(width)
        self.b = Signal(width)
        self.r = Signal(width + 1)
        self.d = domain

    def elaborate(self, platform):
        m = Module()
        m.domain[self.d] += self.r.eq(self.a + self.b)
        return m

# if __name__ == '__main__':
#     m = Adder(10, 'sync')
#     ports = [m.a, m.b, m.r]
#     main(m, platform=None, ports=ports)

if __name__ == '__main__':
    m = Adder(10, 'sync')
    ports = [m.a, m.b, m.r]
    fragment = Fragment.get(m, None)
    output = rtlil.convert(fragment, name='top', ports=ports)
    with open('adder.il', 'w') as f:
        f.write(output)
    subprocess.run('make')


    import random
    import simulation as sim

    class Signal():
        def __init__(self, signal_id):
            self.id = signal_id

        @property
        def value(self):
            return sim.get_by_id(self.id)

        @value.setter
        def value(self, value):
            return sim.set_by_id(self.id, value)

        def __str__(self):
            return str(self.value)

    class Dut:
        def __init__(self):
            self.rst = Signal(0)
            self.clk = Signal(1)
            self.r = Signal(2)
            self.b = Signal(3)
            self.a = Signal(4)
        def __str__(self):
            return str(self.rst.value)

    def rising_edge(s):
        prev = s.value
        while True:
            if s.value == 1 and prev == 0:
                return
            prev = s.value
            yield

    def clock(s):
        while True:
            s.value = clk = (s.value + 1) % 2
            yield
            
    def simple_scheduller(coros):
        loop = 0
        start = time.time()
        try:
            while True:
                for coro in coros:
                    next(coro)
                sim.delta()
                loop += 1
        except StopIteration:
            elapsed = time.time() - start
            print(f'loops={loop} elapsed={elapsed}')
            return
            

    dut = Dut()

    def coroutine():
        yield from rising_edge(dut.clk)
        dut.rst.value = 1
        yield from rising_edge(dut.clk)
        dut.rst.value = 0
        yield from rising_edge(dut.clk)

        for _ in range(1000):
            dut.a.value = a = random.randint(0, 255)
            dut.b.value = b = random.randint(0, 255)

            yield from rising_edge(dut.clk)
            yield from rising_edge(dut.clk)

            assert dut.a.value + dut.b.value == dut.r.value


    clock_coro = clock(dut.clk)
    main_coro = coroutine()
    simple_scheduller([clock_coro, main_coro])
