from .dut import Dut
from .gen import generate_wrapper
import importlib
from nmigen import *
from nmigen._toolchain import require_tool
from nmigen.back import rtlil, verilog
import os
import subprocess
import sys
import tempfile


class Simulator:
    def __init__(self, design, platform=None, ports=None, debug=False, vcd=False):
        fragment = Fragment.get(design, None)
        output = rtlil.convert(fragment, name='top', ports=ports)

        self.tmp_dir = tempfile.TemporaryDirectory()
        self.il_file = self.tmp_dir.name + '/top.il'
        self.cpp_file = self.tmp_dir.name + '/top.cc'
        self.so_file = self.tmp_dir.name + '/simulation.so'

        wrapper_path = os.path.dirname(os.path.realpath(__file__))
        self.wrapper_file = wrapper_path + '/wrapper.cc.j2'
        self.debug = debug
        self.vcd = vcd

        with open(self.il_file, 'w') as f:
            f.write(output)

        self.cxxrtl()
        self.add_wrapper()
        self.build()

        spec = importlib.util.spec_from_file_location("simulation", self.so_file)
        self.sim = importlib.util.module_from_spec(spec)
        self.dut = Dut(self.sim)

    def cxxrtl(self):
        subprocess.run(f'yosys -q {self.il_file} -o {self.cpp_file}'.split(' ')
                      ).check_returncode()

    def add_wrapper(self):
        with open(self.wrapper_file) as f:
            wrapper = f.read()
        with open(self.cpp_file, 'r+') as f:
            cpp = f.read()
            wrapper = generate_wrapper(cpp=cpp, template=wrapper)
            f.write(wrapper)

    def build(self):
        python_cflags = subprocess.check_output(['python3-config', '--includes'], encoding="utf-8")
        debug_cflags = '-DDEBUG' if self.debug else ''
        vcd_cflags = '-DVCD_DUMP' if self.vcd else ''
        subprocess.run(['clang++',
                        '-I/usr/local/share/yosys/include/backends/cxxrtl/',
                        '-shared', '-fPIC', '-O3',
                        *python_cflags.split(' '),
                        debug_cflags,
                        vcd_cflags,
                        '-o', f'{self.so_file}',
                        f'{self.cpp_file}']).check_returncode()

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

    def set_precision(self, value, units='ps'):
        mult = 1 if units == 'ps' else \
               10**3 if units == 'ns' else \
               10**6 if units == 'us' else \
               10**9 if units == 's' else \
               None
        if not mult:
            raise ValueError('Invalid unit')
        return self.sim.set_sim_time_precision(value * mult)


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
