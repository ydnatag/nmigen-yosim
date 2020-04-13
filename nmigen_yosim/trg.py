class TRIGGERS:
    OTHER = 0
    TIMER = 1
    EDGE = 2
    R_EDGE = 3
    F_EDGE = 4

def edge(s):
    return TRIGGERS.EDGE, s.id

def falling_edge(s):
    return TRIGGERS.F_EDGE, s.id

def rising_edge(s):
    return TRIGGERS.R_EDGE, s.id

def timer(time):
    return TRIGGERS.TIMER, time

def clock(clk, period=10):
    period_2 = int(period / 2)
    while True:
        yield timer(period_2)
        clk.value = (clk.value + 1) % 2
