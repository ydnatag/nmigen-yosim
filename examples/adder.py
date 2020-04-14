from nmigen import *
from nmigen_yosim import *
from nmigen_yosim.vcd import VCDWaveformWriter
import random
import time

class Add(Elaboratable):
    def __init__(self, width):
        self.a = Signal(width)
        self.b = Signal(width)
        self.r = Signal(width + 1)

    def elaborate(self, platform):
        m = Module()
        m.d.comb += self.r.eq(self.a + self.b)
        return m

class Adder(Elaboratable):
    def __init__(self, width, domain='comb'):
        self.width = width
        self.a = Signal(width)
        self.b = Signal(width)
        self.r = Signal(width + 1)
        self.d = domain

    def elaborate(self, platform):
        m = Module()
        m.submodules.adder = add = Add(self.width)
        m.d.comb += add.a.eq(self.a)
        m.d.comb += add.b.eq(self.b)
        m.domain[self.d] += self.r.eq(add.r)
        return m

def reset_coroutine(rst, clk):
    rst.value = 1
    yield rising_edge(clk)
    rst.value = 0
    yield rising_edge(clk)

def main_coroutine(sim, dut):
    for j in range(8, 65):
        yield from reset_coroutine(dut.rst, dut.clk)
        for i in range(1000):
            a = random.randint(0, 2**j-1)
            b = random.randint(0, 2**j-1)
            dut.a.value = a
            dut.b.value = b
            yield rising_edge(dut.clk)
            yield rising_edge(dut.clk)
            assert a == dut.a.value, f'{a} == {dut.a.value}'
            assert b == dut.b.value, f'{b} == {dut.b.value}'
            assert a + b == dut.r.value, (
                f'@{sim.sim_time} ps: {a} + {b} == {dut.r.value}')

def run_sim(vcd):
    m = Adder(65, 'sync')
    sim_config = {
        'platform': None,
        'ports': [m.a, m.b, m.r],
        'vcd_file': './dump.vcd' if vcd else None,
        'precision': (5, 'ns')}

    start = time.time()
    with Simulator(m, **sim_config) as (sim, dut):
        clock_coro = clock(dut.clk, 10, 'ns')
        main_coro = main_coroutine(sim, dut)
        sim.run([clock_coro, main_coro])
    elapsed = time.time() - start

    print(f'\nResults (vcd={vcd}):')
    print(f'sim time: {sim.sim_time / 1000} ns')
    print(f'real time: {elapsed} s')
    print(f'simtime / realtime: {sim.sim_time / 1000 / elapsed} ns/s')

if __name__ == '__main__':
    run_sim(vcd=True)
    run_sim(vcd=False)
