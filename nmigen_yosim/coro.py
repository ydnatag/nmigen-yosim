from .trg import timer

def clock(clk, period=10, units='ps'):
    period_2 = int(period / 2)
    while True:
        yield timer(period_2, units=units)
        clk.value = (clk.value + 1) % 2
