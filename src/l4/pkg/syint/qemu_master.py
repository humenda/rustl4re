import os
import re
import signal
import subprocess
import sys
import time
from tempfile import NamedTemporaryFile

from asyncproc import Process

BEGIN_MARKER = "[[|| BEGIN TESTS ||]]"
END_MARKER = "[[|| END TESTS ||]]"

def read_makefile():
    """Read the makefile to extract modname and l4dir."""
    # this will only work for simple makefiles
    with open(os.path.join('src', 'Makefile')) as m:
        vars = {key.strip(' \n\t?'): val.strip()  for key, val in
                [line.split('=') for line in m.readlines() if '=' in line]}
    for missing in (m for m in ('TARGET', 'L4DIR', 'PKGDIR') if m not in vars):
        print("%s missing in Makefile, please fix this script or the Makefile."
                \
                        % missing)
        sys.exit(123)
    l4dir = vars['L4DIR']
    if 'PKGDIR' in l4dir:
        l4dir = l4dir.replace('$(PKGDIR)', vars['PKGDIR'])
    return (vars['TARGET'], l4dir)

def run_qemu(modname, l4dir):
    # execute make to start qemu with the given module;  use preexec_fn=setsid
    # to start the subprocess in a process group, which allows us to control
    # (and kill) all children at once.
    import subprocess
    proc = Process(['make', '-C', l4dir, 'qemu', '--no-print-directory',
            'E=' + modname], preexec_fn=os.setsid,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    data, errors = ('', '')
    exit_code = None
    time_slept = 0
    while exit_code is None and END_MARKER not in data and time_slept <= 10:
        # check to see if process has ended
        exit_code = proc.wait(os.WNOHANG)
        # print any new output
        stdin, stdout = proc.readboth()
        data += stdin
        errors += stdout
        time.sleep(0.5)
        time_slept += 0.5
    data += proc.read()
    if END_MARKER not in data:
        print("Unsuccessful termination, program output:\n{}\n{}".format(
                    data, errors))
        sys.exit(1)
    try:
        pgrp = os.getpgid(proc.pid())
        os.killpg(pgrp, signal.SIGINT)
    except OSError:
        pass
    # strip\r and colour codes
    return re.sub('\x1b.+?m', '', data.replace('\r', ''))[1:]


def main():
    modname, l4dir = read_makefile()
    # we're one level above that makefile, strip one directory
    l4dir = os.path.dirname(l4dir)
    stdout = run_qemu(modname, l4dir)
    # we can assume to find both BEGIN_MARKER and END_MARKER
    test_results = stdout[stdout.find(BEGIN_MARKER) + len(BEGIN_MARKER):
            stdout.find(END_MARKER)] # strip boundary markers
    # strip logging prefix, if presend
    test_results = '\n'.join(re.sub(r'^%s\s+\|\s' % modname, '', line)
            for line in test_results.split('\n')).strip()
    print(test_results)
#    with NamedTemporaryFile() as file:
#        file.write(test_results.encode('utf-8'))
#        file.seek(0)
#        proc = subprocess.Popen(['prove', file.name], stdin=subprocess.PIPE)
#        proc.communicate(test_results.encode(sys.getdefaultencoding()))
#        proc.wait()

if __name__ == '__main__':
    main()
