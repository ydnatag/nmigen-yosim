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

PERIOD = 10000
def reset_coroutine(rst, clk):
    rst.value = 1
    yield rising_edge(clk)
    rst.value = 1
    yield rising_edge(clk)
    rst.value = 0

def print_on_rising_edge(clk):
    while True:
        yield rising_edge(clk)

def main_coroutine(sim, dut):
    yield from reset_coroutine(dut.rst, dut.clk)
    coro = sim.fork(print_on_rising_edge(dut.clk))
    # yield from sim.join(coro) # Testing coroutine join
    for j in range(2, 64):
        print(f'width <= {j}')
        for i in range(100):
            a = random.randint(0, 2**j-1)
            dut.a.value = a
            b = random.randint(0, 2**j-1)
            dut.b.value = b
            yield rising_edge(dut.clk)
            yield rising_edge(dut.clk)
            assert a == dut.a.value, f'{a} == {dut.a.value}'
            assert b == dut.b.value, f'{b} == {dut.b.value}'
            #print(f'@ {sim.sim_time} ps: {dut.a.value} + {dut.b.value} == {dut.r.value}')
            assert a + b == dut.r.value, f'{a} + {b} == {dut.r.value}'

if __name__ == '__main__':
    m = Adder(64, 'sync')
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
