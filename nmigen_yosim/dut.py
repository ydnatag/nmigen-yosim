class Signal():
    def __init__(self, simulation, signal_id, name):
        self.sim = simulation
        self.id = signal_id
        self.name = name
        self.width = self.sim.get_width_by_id(self.id)

    @property
    def value(self):
        return self.sim.get_by_id(self.id)

    @value.setter
    def value(self, value):
        return self.sim.set_by_id(self.id, value)

    def __len__(self):
        return self.width

    def __str__(self):
        return str(self.value)

class Module():
    def __init__(self, name):
        self.name = name

    def add_module(self, hierarchy):
        if hierarchy:
            module = hierarchy[0]
            if not hasattr(self, module):
                setattr(self, module, Module(module))
            mod = self.add_module(hierarchy[1:]) if len(hierarchy) > 1 \
                  else getattr(self, module)
        else:
            mod = self
        return mod
    def get_signals(self, recursive=False):
        signals = []
        for name in dir(self):
            attr = getattr(self, name)
            if isinstance(attr, Signal):
                signals.append(attr)
            elif isinstance(attr, Module):
                if recursive:
                    signals.extend(attr.get_signals())
        return signals

class Dut(Module):
    def __init__(self, simulation):
        self.name = 'top'
        for i in range(simulation.n_of_signals()):
            name = simulation.get_signal_name(i)
            hierarchy = name.split('.')[1:]
            signal = hierarchy[-1]
            hierarchy = hierarchy[:-1]
            mod = self.add_module(hierarchy)
            setattr(mod, signal, Signal(simulation, i, name))
