from .dut import Dut
import importlib
from nmigen import *
from nmigen._toolchain import require_tool
from nmigen.back import rtlil, verilog
import subprocess
import sys
import os

class Simulator:
    def __init__(self, design, platform=None, ports=None, debug=False):
        fragment = Fragment.get(design, None)
        output = rtlil.convert(fragment, name='top', ports=ports)

        self.tmp_dir = tempfile.TemporaryDirectory()
        self.il_file = self.tmp_dir.name + '/top.il'
        self.cpp_file = self.tmp_dir.name + '/top.cc'
        self.so_file = self.tmp_dir.name + '/simulation.so'

        wrapper_path = os.path.dirname(os.path.realpath(__file__))
        self.wrapper_file = wrapper_path + '/wrapper.cc'
        self.debug = debug

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
        with open(self.cpp_file, 'a') as cpp:
            with open(self.wrapper_file, 'r') as w:
                cpp.write(w.read())

    def build(self):
        python_cflags = subprocess.check_output(['python3-config', '--includes'], encoding="utf-8")
        debug_cflags = '-DDEBUG' if self.debug else ''
        subprocess.run(['clang++',
                        '-I/usr/local/share/yosys/include/backends/cxxrtl/',
                        '-shared', '-fPIC', '-O3',
                        *python_cflags.split(' '),
                        debug_cflags,
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
