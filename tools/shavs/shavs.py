import hashlib
import pathlib
import re
import struct
import subprocess
import tqdm
import zipfile as zip

class Hash:
    def digest(self, msg):
        raise NotImplementedError()

class HashlibHash(Hash):
    def __init__(self, factory):
        self.factory = factory

    def digest(self, msg):
        return self.factory(msg).digest()

class ProcessHash(Hash):
    def __init__(self, cmd):
        self.proc = subprocess.Popen(cmd,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def write(self, msg):
        o = 0
        while o < len(msg):
            n = self.proc.stdin.write(msg[o:])
            assert n > 0
            o += n
        self.proc.stdin.flush()

    def read(self, n):
        b = b''
        while len(b) < n:
            d = self.proc.stdout.read(n - len(b))
            assert len(d)
            b += d
        return b

    def digest(self, msg):
        self.write(struct.pack('I', len(msg)) + msg)
        return self.read(32)

class SHAVS:
    class MsgTest:
        def __init__(self, m, h):
            self.m = m
            self.h = h

        def steps(self):
            return 1

        def run(self, hobj, pb):
            assert hobj.digest(self.m) == self.h
            pb.update(1)

    class MonteCarloTest:
        def __init__(self, seed):
            self.seed = seed
            self.mds = list()

        def steps(self):
            return 1000 * 100

        def addmd(self, ct, md):
            assert len(self.mds) == ct
            self.mds.append(md)

        def run(self, hobj, pb):
            h = self.seed
            for i in range(100):
                mc_1 = h
                mc_2 = h
                for _ in range(1000):
                    msg = mc_2 + mc_1 + h
                    mc_2 = mc_1
                    mc_1 = h
                    h = hobj.digest(msg)
                    pb.update(1)
                assert h == self.mds[i]

    def __init__(self, zipfile, prefix, hobj):
        self.hobj = hobj
        self.tests = list()
        with zip.ZipFile(zipfile) as zf:
            for fn in zf.namelist():
                p = pathlib.Path(fn)
                if p.suffix == '.rsp' and p.name.startswith(prefix):
                    with zf.open(fn) as f:
                        d = {}
                        for l in f:
                            m = re.match(r'(\w+)\s*=\s*(\w+)', l.decode())
                            if m:
                                k,v = m.groups()
                                if k == 'MD':
                                    if 'Seed' in d:
                                        # new Monte Carlo
                                        self.tests.append(SHAVS.MonteCarloTest(
                                            bytes.fromhex(d['Seed'])))
                                    if 'COUNT' in d:
                                        self.tests[-1].addmd(int(d['COUNT']), bytes.fromhex(v))
                                    else:
                                        self.tests.append(SHAVS.MsgTest(
                                            bytes.fromhex(d['Msg'])[:int(d['Len'])//8],
                                            bytes.fromhex(v)))
                                    d.clear()
                                else:
                                    d[k] = v

    def run(self):
        steps = sum(t.steps() for t in self.tests)
        with tqdm.tqdm(total=steps) as pb:
            for t in self.tests:
                t.run(self.hobj, pb)
        print('All tests passed.')

#SHAVS('shabytetestvectors.zip', 'SHA256', HashlibHash(hashlib.sha256)).run()
SHAVS('shabytetestvectors.zip', 'SHA256', ProcessHash('../../src/common/sha2test')).run()
