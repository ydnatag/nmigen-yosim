class TRIGGERS:
    DELTA = 0
    TIMER = 1
    EDGE = 2
    R_EDGE = 3
    F_EDGE = 4

def delta():
    return TRIGGERS.DELTA, 0

def edge(s):
    return TRIGGERS.EDGE, s.id

def falling_edge(s):
    return TRIGGERS.F_EDGE, s.id

def rising_edge(s):
    return TRIGGERS.R_EDGE, s.id

def timer(time, units='ps'):
    mult = 1 if units == 'ps' else \
           10**3 if units == 'ns' else \
           10**6 if units == 'us' else \
           10**9 if units == 's' else \
           None
    if not mult:
        raise ValueError('Invalid unit')
    return TRIGGERS.TIMER, int(time * mult)
