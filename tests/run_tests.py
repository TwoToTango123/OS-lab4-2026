#!/usr/bin/env python3
import os
from pathlib import Path
import random
import re
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PARENT = ROOT / 'build/parent'
ENV = os.environ.copy()
passed = 0

def run(data, cwd, ok=True):
    p = subprocess.Popen([str(PARENT)], cwd=cwd, env=ENV,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, start_new_session=True)
    try:
        output, _ = p.communicate(data, timeout=15)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
        p.communicate()
        raise AssertionError('program hung')
    assert (p.returncode == 0) == ok, output.decode(errors='replace')
    for pid in re.findall(rb'started child\d, PID=(\d+)', output):
        assert not Path('/proc/' + pid.decode()).exists(), 'child was not reaped'
    return output

def test(name, fn):
    global passed
    fn()
    passed += 1
    print('PASS:', name)

def check_lines(lines):
    with tempfile.TemporaryDirectory(prefix='lab4-test-') as d:
        data = 'a.txt\nb.txt\n' + ''.join(lines)
        run(data.encode('ascii'), d)
        expected = ['', '']
        for raw in lines:
            has_newline = raw.endswith('\n')
            text = raw[:-1] if has_newline else raw
            expected[len(text) > 10] += text[::-1] + ('\n' if has_newline else '')
        for name, value in zip(['a.txt', 'b.txt'], expected):
            assert (Path(d)/name).read_bytes() == value.encode('ascii')

before = set(Path('/tmp').glob('lab4-*'))
test('length 0, 1, 9, 10, 11 and spaces',
     lambda: check_lines(['\n', 'a\n', '123456789\n', '1234567890\n',
                          '12345678901\n', 'a b c\n']))
test('ASCII letters, punctuation and tabs',
     lambda: check_lines(['Hello OS!\n', 'a\tb c?!\n', 'abcdefghij\n', 'abcdefghijk\n']))
test('last line without newline', lambda: check_lines(['first\n', 'abcdef123456']))
test('EOF without data', lambda: check_lines([]))
test('line of 1,000,000 characters', lambda: check_lines(['abcd'*250000+'\n']))
random.seed(20)
lines = [''.join(random.choices('abc XYZ0123!?.', k=random.randint(0, 300)))+'\n'
         for _ in range(500)]
test('500 random lines, five repeated runs',
     lambda: [check_lines(lines) for _ in range(5)])
with tempfile.TemporaryDirectory(prefix='lab4-test-') as d:
    test('missing file names', lambda: run(b'', d, False))
    test('invalid output path', lambda: run(b'/no/such/lab4/a\nb.txt\n', d, False))
    test('same output file', lambda: run(b'a.txt\na.txt\nhello\n', d, False))
    a = Path(d)/'a.txt'
    b = Path(d)/'alias.txt'
    a.write_bytes(b'keep this')
    b.hardlink_to(a)
    def hardlink():
        run(b'a.txt\nalias.txt\n', d, False)
        assert a.read_bytes() == b'keep this'
    test('hardlink alias rejected before truncation', hardlink)
    test('child write error /dev/full, no deadlock',
         lambda: run(b'/dev/full\nb.txt\nhello\n', d, False))
    def crash_child():
        p = subprocess.Popen([str(PARENT)], cwd=d, env=ENV, text=True,
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, start_new_session=True)
        try:
            p.stdin.write('a.txt\nb.txt\n')
            p.stdin.flush()
            seen = ''
            while 'started child2' not in seen:
                seen += p.stdout.readline()
            pids = [int(s) for s in re.findall(r'started child\d, PID=(\d+)', seen)]
            os.kill(pids[0], signal.SIGKILL)
            p.stdin.write('hello\n')
            p.stdin.flush()
            tail, _ = p.communicate(timeout=5)
            assert p.returncode != 0
            for pid in pids:
                assert not Path(f'/proc/{pid}').exists()
        finally:
            if p.poll() is None:
                os.killpg(p.pid, signal.SIGKILL)
                p.communicate()
    test('forced child termination, cleanup of both children', crash_child)
assert set(Path('/tmp').glob('lab4-*')) == before, 'temporary IPC file leaked'
print(f'ALL {passed} TESTS PASSED; no child processes or IPC paths left')
