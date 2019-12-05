from nmigen import *
import subprocess
from yosym import Simulator, clock, rising_edge
import random
import time



class Adder(Elaboratable):
    def __init__(self, width, domain='comb'):
        self.width = width
        self.a = Signal(width)
        self.b = Signal(width)
        self.r = Signal(width + 1)
        self.d = domain

    def elaborate(self, platform):
        m = Module()
        m.domain[self.d] += self.r.eq(self.a + self.b)
        return m

PERIOD = 10
def reset_coroutine(rst, clk):
    yield from rising_edge(clk)
    assert sim.sim_time == PERIOD

    yield from rising_edge(clk)
    assert sim.sim_time == 2 * PERIOD
    rst.value = 0

def main_coroutine(sim, dut):
    yield from reset_coroutine(dut.rst, dut.clk)
    # coro = sim.fork(reset_coroutine(dut.rst, dut.clk))
    # yield from sim.join(coro) # Testing coroutine join

    for i in range(1000):
        dut.a.value = random.randint(0, 255)
        dut.b.value = random.randint(0, 255)

        yield from rising_edge(dut.clk)
        assert sim.sim_time == 3 * PERIOD + 2 * PERIOD * i

        yield from rising_edge(dut.clk)
        assert sim.sim_time == 3 * PERIOD + i * 2 * PERIOD + PERIOD

        assert dut.a.value + dut.b.value == dut.r.value


if __name__ == '__main__':
    m = Adder(10, 'sync')
    ports = [m.a, m.b, m.r]
    with Simulator(m, platform=None, ports=ports) as (sim, dut):
        start = time.time()
        clock_coro = clock(dut.clk, PERIOD)
        main_coro = main_coroutine(sim, dut)
        sim.run([main_coro, clock_coro])
        elapsed = time.time() - start

        print('\nResults:')
        print(f'sim time: {sim.sim_time}')
        print(f'real time: {elapsed}')
        print(f'simtime / realtime: {sim.sim_time / elapsed}')
