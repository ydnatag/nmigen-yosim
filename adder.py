from nmigen import *
import subprocess
from yosym import Simulator, clock, rising_edge
import random

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

        
def coroutine(dut):
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

if __name__ == '__main__':
    m = Adder(10, 'sync')
    ports = [m.a, m.b, m.r]
    
    with Simulator(m, platform=None, ports=ports) as (sim, dut):
        clock_coro = clock(dut.clk)
        main_coro = coroutine(dut)
        sim.run([clock_coro, main_coro])
