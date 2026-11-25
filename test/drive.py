#!/usr/bin/env python3
"""Runs the host build of q2-tictactoe as Q2 Pod would (fake /dev/fb0 from fbshim.so, input as text
datagrams on /tmp/q2-tictactoe.input.sock, TERM to end it) through one scenario, and prints what it
logged and the state it ended in. Frames are saved as OUT_DIR/<scenario>-<name>.raw (view.py shows
them).

Usage: drive.py BUILD_DIR OUT_DIR SCENARIO
Scenarios: menu, two (two players by wheel and Centre), touch (two players by touch), q2 (vs the Q2)"""
import os, shutil, signal, socket, subprocess, sys, tempfile, time

build, outdir, scenario = sys.argv[1:4]
build, outdir = os.path.abspath(build), os.path.abspath(outdir)
SOCK = '/tmp/q2-tictactoe.input.sock'
card = tempfile.mkdtemp()
app = os.path.join(card, '.tictactoe')
os.mkdir(app)
shutil.copy(os.path.join(build, 'q2-tictactoe'), app)
dump = os.path.join(card, 'fb.raw')
env = dict(os.environ, LD_PRELOAD=os.path.join(build, 'fbshim.so'), FB_DUMP=dump,
           FB_FINAL=os.path.join(card, 'fb.final'), TT_STATE='1', TT_SEED='7')
if os.path.exists(SOCK):
    os.unlink(SOCK)
proc = subprocess.Popen([os.path.join(app, 'q2-tictactoe')], env=env, cwd=app)
log = os.path.join(app, 'tictactoe.log')

def wait_for(cond, secs, what):
    end = time.time() + secs
    while time.time() < end:
        if cond():
            return
        time.sleep(0.03)
    proc.kill()
    sys.exit(f'FAIL: no {what} after {secs} s\n' + (open(log).read() if os.path.exists(log) else ''))

def state():
    out = {}
    try:
        for line in open(os.path.join(app, 'state.txt')):
            k, v = line.rsplit(' ', 1)
            out[k] = int(v)
    except (OSError, ValueError):
        pass
    return out

wait_for(lambda: os.path.exists(SOCK), 5, 'input socket')
wait_for(lambda: state(), 5, 'first frame')
s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
send = lambda m: s.sendto(m.encode(), SOCK)
CENTRE, RETURN = 218, 170

def key(code, hold=0.05):
    send(f'k d {code}'); time.sleep(hold); send(f'k u {code}'); time.sleep(0.12)

def wheel(steps):  # + clockwise, one step each
    for _ in range(abs(steps)):
        send(f'w {-28 if steps > 0 else 28}'); time.sleep(0.04)
    time.sleep(0.08)

def tap(x, y):
    send(f't d {x} {y}'); time.sleep(0.05); send(f't u {x} {y}'); time.sleep(0.12)

def cell_xy(i):  # the centre of square i, as game.c lays them out
    cs, bx, by = 78, (375 - 3 * 78) // 2, 43
    return bx + (i % 3) * cs + cs // 2, by + (i // 3) * cs + cs // 2

def snap(name):
    time.sleep(0.1)
    for _ in range(50):
        data = open(dump, 'rb').read()
        if len(data) == 320 * 375 * 4:
            break
        time.sleep(0.01)
    open(os.path.join(outdir, f'{scenario}-{name}.raw'), 'wb').write(data)

def go_to_cell(target):  # by the wheel: clockwise steps round the empty squares
    for _ in range(12):
        if state()['cursor'] == target:
            return
        wheel(1)
    sys.exit(f'FAIL: cursor never reached {target}')

def board():
    st = state()
    return [st[f'b{i}'] for i in range(9)]

results = {}
time.sleep(0.4)
if scenario == 'menu':
    snap('title')
    wheel(2); snap('level'); results['sel_level'] = state()['menu']
    lv = state()['level']
    key(CENTRE); results['level_changed'] = (state()['level'] - lv) % 3
    key(CENTRE); key(CENTRE)  # back round to where it was
    wheel(-1); results['sel_two'] = state()['menu']
    wheel(-1); snap('vsq2')
    key(CENTRE); time.sleep(0.2); results['screen'] = state()['screen']
    # a held Return repeats its downs: one trip to the menu, not a toggle
    send(f'k d {RETURN}')
    for _ in range(5):
        time.sleep(0.06); send(f'k d {RETURN}')
    send(f'k u {RETURN}'); time.sleep(0.2)
    results['after_return'] = state()['screen']
elif scenario == 'two':
    wheel(1); key(CENTRE); time.sleep(0.2)
    results['mode'] = state()['mode']
    snap('empty')
    # X: 4, O: 0, X: 2, O: 6, X: 3?  -> X takes 4, 2, then 6 is O's... play X to win the 2-4-6 diagonal
    for cell in (4, 0, 2, 1, 6):
        go_to_cell(cell)
        if cell == 6:
            snap('cursor')
        key(CENTRE)
    time.sleep(0.15); snap('placing')
    time.sleep(1.0); snap('x-wins')
    results['over'] = state()['over']
    results['x_wins'] = state()['x_wins']
    key(CENTRE); time.sleep(0.2)
    results['next_turn'] = state()['turn']  # O starts the second round
    results['next_empty'] = sum(board())
elif scenario == 'touch':
    wheel(1); key(CENTRE); time.sleep(0.2)
    # a draw: X O X / X O O / O X X
    for cell in (0, 1, 2, 4, 3, 5, 7, 6, 8):
        tap(*cell_xy(cell))
    time.sleep(0.8); snap('draw')
    results['over'] = state()['over']
    results['draws'] = state()['draws']
    tap(187, 160); time.sleep(0.2)
    results['new_round'] = sum(board())
elif scenario == 'q2':
    key(CENTRE); time.sleep(0.2)
    results['mode'] = state()['mode']
    rounds, q2_moves, end_states = 0, 0, []
    while rounds < 4:
        st = state()
        if st['over']:
            if rounds == 0:
                time.sleep(1.0); snap('end')
            end_states.append(st['over'])
            rounds += 1
            time.sleep(0.4); key(CENTRE); time.sleep(0.2)
            continue
        if st['thinking']:
            if q2_moves == 0:
                snap('thinking')
            before = sum(1 for v in board() if v == 2)
            wait_for(lambda: not state()['thinking'] or state()['over'], 3, 'Q2 move')
            q2_moves += sum(1 for v in board() if v == 2) > before
            continue
        if st['turn'] != 1:
            time.sleep(0.05)
            continue
        # play the first empty square by touch
        tap(*cell_xy(board().index(0)))
        time.sleep(0.1)
    results['q2_moves'] = q2_moves
    results['rounds'] = rounds
    st = state()
    results['tally'] = st['x_wins'] + st['o_wins'] + st['draws']

end = state()
proc.send_signal(signal.SIGTERM)
try:
    status = proc.wait(5)
except subprocess.TimeoutExpired:
    proc.kill()
    status = 'killed'
print(open(log).read())
sav = os.path.join(app, 'tictactoe.sav')
print('save:', open(sav).read().replace('\n', ' | ') if os.path.exists(sav) else '-')
print('exit status:', status)
print('panel:', open(os.path.join(card, 'fb.final')).read().strip() if os.path.exists(os.path.join(card, 'fb.final')) else '-')
print('socket left:', os.path.exists(SOCK))
for k, v in end.items():
    print(f'end: {k}: {v}')
for k, v in results.items():
    print(f'result: {k}: {v}')
shutil.rmtree(card)
