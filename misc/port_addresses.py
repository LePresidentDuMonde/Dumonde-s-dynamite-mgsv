"""Port hook addresses between mgsvtpp.exe builds.

Phase 1, signatures: masked byte signatures (call targets / RIP-relative offsets are wildcards) from the old exe
are searched in the new exe. Data addresses: exact bytes, or code that references the data is ported and its
RIP-relative operand followed.

Phase 2, layout: the 1.0.15.3 protector moved most functions into its own section, rewrote their code and left a
`jmp` stub at each original address. Function starts are mapped by original address: offsets between builds are
taken from functions with unique signatures, a prediction is accepted when neighbours agree or exactly one
candidate is a function start in the new exe, and the candidate's calls must match.

Usage: python misc/port_addresses.py <old mgsvtpp.exe> <new mgsvtpp.exe> <new version, e.g. 1.0.15.5> [old .dmp]
The old exe must be the build the current table (TABLE) was made for. Use a memory dump of the running old exe
when its code is encrypted on disk (Task Manager, Details, mgsvtpp.exe, Create dump file).
Writes hook/src/mgsvtpp_addresses_new.h: review it, rename it, point TABLE and dynamite.cpp at it. Prints the patch
site mapping for hook/src/dynamitePatches.cpp; NEW BYTES means the expected bytes there must be updated.
Optional but recommended: capstone (pip install capstone) maps patch sites whose surrounding code was rewritten
and checks every function start by its instructions.
"""
import bisect
import difflib
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / 'hook/src/mgsvtpp_addresses_1_0_15_4_en.h'
PATCHES = ROOT / 'hook/src/dynamitePatches.cpp'
OUT = ROOT / 'hook/src/mgsvtpp_addresses_new.h'
CACHE = ROOT / 'misc/port_results.json'  # signature pass results, reused between runs

EXEC = 0x20000000
# opcodes (incl. 0F xx second bytes) commonly followed by a RIP-relative ModRM
RIP_OPS = set(range(0x00, 0x40)) | {0x63, 0x69, 0x6B, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8D,
                                    0x80, 0x81, 0x83, 0xC6, 0xC7, 0xF6, 0xF7, 0xFE, 0xFF, 0xD8, 0xD9, 0xDD,
                                    0x51, 0x54, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                                    0x6E, 0x6F, 0x7E, 0x7F, 0xB6, 0xB7, 0xBE, 0xBF, 0xC2, 0xD6, 0xE6, 0xE7, 0xEF}
LENGTHS = (32, 48, 64, 96, 128, 192)
BACKOFFS = (0, 16, 32, 64, 128)
FUZZY_LEN = 96
FUZZY_MIN = 0.8  # share of non-wildcard bytes that must be equal
FUZZY_GAP = 0.15  # required lead over the second best candidate
CALLS_MIN = 0.5  # phase 2: share of translated calls the candidate must also make
WINDOW = 0x800  # phase 2: search range around predicted address
WINDOW_MIN = 0.85  # phase 2: required code similarity or call share in window search
TABLE_VOTES = 3  # phase 2: neighbour slots that must agree on a vtable entry
VERIFY_LEN = 60  # final check: instructions compared
VERIFY_GOOD, VERIFY_BAD = 0.95, 0.8  # replace a result below BAD by an alternative above GOOD
NEAR_MAX = 0x10000  # duplicates: max distance from the address predicted by neighbour shift


def read_dump_image(dump_path, exe_name, timestamp):
    """main module memory from a full minidump (Task Manager "Create dump file"), laid out by RVA"""
    with open(dump_path, 'rb') as f:
        sig, _, nstreams, dir_rva = struct.unpack('<4sIII', f.read(16))
        if sig != b'MDMP':
            raise ValueError(f'{dump_path} is not a minidump')
        f.seek(dir_rva)
        streams = {t: rva for t, _, rva in (struct.unpack('<III', f.read(12)) for _ in range(nstreams))}
        if 4 not in streams or 9 not in streams:
            raise ValueError('dump has no module list or full memory, create a full dump')

        f.seek(streams[4])
        module = None
        for i in range(struct.unpack('<I', f.read(4))[0]):
            f.seek(streams[4] + 4 + 108 * i)
            base, size, _, ts, name_rva = struct.unpack('<QIIII', f.read(24))
            pos = f.tell()
            f.seek(name_rva)
            name = f.read(struct.unpack('<I', f.read(4))[0]).decode('utf-16-le')
            f.seek(pos)
            if name.lower().endswith(exe_name.lower()):
                module = base, size, ts, name
                break
        if not module:
            raise ValueError(f'{exe_name} not found in dump')
        base, size, ts, name = module
        print(f'dump module {name} base {base:#x} timestamp {ts:#x}')
        if ts != timestamp:
            raise ValueError(f'dump is from a different build ({ts:#x}), expected {timestamp:#x}')

        f.seek(streams[9])
        count, data_rva = struct.unpack('<QQ', f.read(16))
        ranges = [struct.unpack('<QQ', f.read(16)) for _ in range(count)]
        image = bytearray(size)
        for start, n in ranges:
            lo, hi = max(start, base), min(start + n, base + size)
            if lo < hi:
                f.seek(data_rva + lo - start)
                image[lo - base:hi - base] = f.read(hi - lo)
            data_rva += n
        return bytes(image)


class PE:
    def __init__(self, path, dump=None):
        """dump: minidump of the running exe, used instead of file contents (packed/encrypted code)"""
        self.data = self.file = Path(path).read_bytes()
        self.dump = dump
        d = self.data
        nt = struct.unpack_from('<I', d, 0x3C)[0]
        nsec, = struct.unpack_from('<H', d, nt + 6)
        self.timestamp, = struct.unpack_from('<I', d, nt + 8)
        opt_size, = struct.unpack_from('<H', d, nt + 20)
        self.base, = struct.unpack_from('<Q', d, nt + 24 + 24)
        self.sections = []
        for i in range(nsec):
            o = nt + 24 + opt_size + 40 * i
            vsize, va, raw_size, raw = struct.unpack_from('<IIII', d, o + 8)
            chars, = struct.unpack_from('<I', d, o + 36)
            # in memory image file offset == rva
            self.sections.append((self.base + va, va, vsize, chars) if dump else (self.base + va, raw, raw_size, chars))
        if dump:
            self.data = read_dump_image(dump, Path(path).name, self.timestamp)

    def off(self, va):
        for sva, raw, size, _ in self.sections:
            if sva <= va < sva + size:
                return raw + va - sva
        return None

    def va(self, off):
        for sva, raw, size, _ in self.sections:
            if raw <= off < raw + size:
                return sva + off - raw
        return None

    def is_code(self, va):
        return any(sva <= va < sva + size and ch & EXEC for sva, _, size, ch in self.sections)

    def functions(self):
        """code ranges from the exception directory: sorted begin, end and owning function (chained parts resolved)"""
        nt = struct.unpack_from('<I', self.file, 0x3C)[0]
        rva, size = struct.unpack_from('<II', self.file, nt + 24 + 112 + 8 * 3)
        o = self.off(self.base + rva)
        rows = []
        for i in range(0, size - 11, 12):
            b, e, unwind = struct.unpack_from('<III', self.data, o + i)
            owner = b
            u = self.off(self.base + (unwind & ~1))
            for _ in range(8):  # UNW_FLAG_CHAININFO: parent entry follows unwind codes
                if u is None or not self.data[u] >> 3 & 4:
                    break
                n = self.data[u + 2]
                owner, _, unwind = struct.unpack_from('<III', self.data, u + 4 + 2 * (n + (n & 1)))
                u = self.off(self.base + unwind)
            rows.append((self.base + b, self.base + e, self.base + owner))
        rows.sort()
        return tuple(np.array(c, dtype=np.int64) for c in zip(*rows))

    def owner(self, funcs, va):
        begins, ends, owners = funcs
        i = int(np.searchsorted(begins, va, 'right')) - 1
        return int(owners[i]) if i >= 0 and va < ends[i] else None

    def code_sections(self):
        return [(sva, raw, size) for sva, raw, size, ch in self.sections if ch & EXEC]


def mask(b):
    """1 marks bytes that depend on code/data location"""
    m = bytearray(len(b))
    for i in range(len(b)):
        if b[i] in (0xE8, 0xE9):
            m[i + 1:i + 5] = b'\1' * len(m[i + 1:i + 5])
        elif i > 0 and (b[i] & 0xC7) == 0x05 and b[i - 1] in RIP_OPS:
            m[i + 1:i + 5] = b'\1' * len(m[i + 1:i + 5])
    return m


def pattern(b, m):
    return re.compile(b''.join(b'.' if w else re.escape(bytes([c])) for c, w in zip(b, m)), re.DOTALL)


def search(new, pat, limit=2):
    res = []
    for mt in pat.finditer(new.data):
        if new.is_code(new.va(mt.start()) or 0):
            res.append(mt.start())
            if len(res) >= limit:
                break
    return res


def fuzzy(old, new, va, expected=None):
    """candidates by short prefix, best one by masked byte similarity over FUZZY_LEN bytes;
    equally good candidates (duplicated functions) are resolved by distance to expected address"""
    o = old.off(va)
    b = old.data[o:o + FUZZY_LEN]
    m = mask(b)
    hits = [h for h in search(new, pattern(b[:16], m[:16]), limit=5000) if h + FUZZY_LEN <= len(new.data)]
    if not hits:
        return None, 'no candidates'

    ref = np.frombuffer(b, np.uint8)
    keep = np.frombuffer(bytes(m), np.uint8) == 0
    scores = sorted(((np.count_nonzero((np.frombuffer(new.data, np.uint8, FUZZY_LEN, h) == ref) & keep) / keep.sum(), h)
                     for h in hits), reverse=True)
    best, h = scores[0]
    second = scores[1][0] if len(scores) > 1 else 0
    if best >= FUZZY_MIN and best - second >= FUZZY_GAP:
        return new.va(h), f'fuzzy{best:.0%}/{second:.0%}'
    if best >= FUZZY_MIN and expected is not None:
        near = sorted((abs(new.va(c) - expected), c) for sc, c in scores if sc >= best - 0.02)
        if near[0][0] <= NEAR_MAX and (len(near) == 1 or near[1][0] > NEAR_MAX):
            return new.va(near[0][1]), f'fuzzy-near{best:.0%} x{len(near)}'
    return None, f'ambiguous {best:.0%}/{second:.0%}'


def port_thunk(old, new, va, depth):
    """jmp stub: port its target, then find the only stub in new jumping there"""
    o = old.off(va)
    target = va + 5 + struct.unpack_from('<i', old.data, o + 1)[0]
    nt, how = port_code(old, new, target, None, depth + 1)
    if nt is None:
        return None, f'thunk target {target:#x}: {how}'
    stubs = [x - 1 for x in find_xrefs(new, nt, limit=64) if new.data[x - 1] == 0xE9]
    if len(stubs) == 1:
        return new.va(stubs[0]), f'thunk({how})'
    return None, f'thunk target ported, {len(stubs)} stubs'


def port_code(old, new, va, hint_delta, depth=0):
    """returns (new va, method) or (None, reason)"""
    o = old.off(va)
    if old.data[o] == 0xE9 and depth < 2:
        res = port_thunk(old, new, va, depth)
        if res[0] is not None:
            return res

    for back in BACKOFFS:
        start = o - back
        # fast path: same shift as the previous resolved address
        new_start = hint_delta is not None and new.off(va - back + hint_delta)
        if new_start:
            b = old.data[start:start + 64]
            if pattern(b, mask(b)).match(new.data, new_start):
                return va + hint_delta, f'delta{back and f"-{back}" or ""}'
        for n in LENGTHS:
            b = old.data[start:start + n]
            hits = search(new, pattern(b, mask(b)))
            if len(hits) == 1:
                return new.va(hits[0] + back), f'sig{n}{back and f"-{back}" or ""}'
            if not hits:
                break
    res = fuzzy(old, new, va, None if hint_delta is None else va + hint_delta)
    if res[0] is None and depth == 0:
        x = port_by_xref(old, new, va)
        return x if x[0] is not None else (None, f'{res[1]}; {x[1]}')
    return res


def find_xrefs(pe, target, limit=8):
    """offsets of 4 byte RIP displacements in code that point at target (assumes disp ends the instruction)"""
    res = []
    chunk = 1 << 22
    for sva, raw, size in pe.code_sections():
        for c in range(0, size - 3, chunk):
            n = min(chunk + 3, size - c)
            a = np.frombuffer(pe.data, dtype=np.uint8, count=n, offset=raw + c).astype(np.int64)
            disp = a[:-3] | a[1:-2] << 8 | a[2:-1] << 16 | a[3:] << 24
            disp -= (disp >= 1 << 31) * (1 << 32)
            idx = np.nonzero(disp + np.arange(c, c + len(disp)) == target - sva - 4)[0]
            res += [raw + c + int(i) for i in idx[:limit - len(res)]]
            if len(res) >= limit:
                return res
    return res


def port_by_xref(old, new, va):
    """port code that references va (call/jmp/RIP-relative operand) and follow the operand in new"""
    votes = Counter()
    for x in find_xrefs(old, va):
        start = x - 24
        b = old.data[start:start + 64]
        m = mask(b)
        m[24:28] = b''
        hits = search(new, pattern(b, m))
        if len(hits) == 1:
            nx = hits[0] + 24
            disp, = struct.unpack_from('<i', new.data, nx)
            votes[new.va(nx) + 4 + disp] += 1
    if not votes:
        return None, 'no xref found'
    (addr, n), = votes.most_common(1)
    if n * 2 <= sum(votes.values()):
        return None, f'xref votes disagree {dict(votes)}'
    return addr, f'xref x{n}/{sum(votes.values())}'


def port_data(old, new, va):
    o = old.off(va)  # None for uninitialized data, it exists only in memory
    b = old.data[o:o + 32] if o is not None else b''
    if len(set(b)) > 4:  # not padding/zeroes, exact bytes may be unique
        hits = [h.start() for h in re.finditer(re.escape(b), new.data)][:2]
        if len(hits) == 1:
            return new.va(hits[0]), 'bytes'
    return port_by_xref(old, new, va)


def owners(pe):
    """function start -> list of its code ranges"""
    res = defaultdict(list)
    for b, e, o in zip(*(c.tolist() for c in pe.functions())):
        res[o].append((b, e))
    return res


def original_addresses(old):
    """moved code -> original address: the protector's `jmp` stub outside any function, in another section"""
    orig = {}
    begins, ends, _ = old.functions()
    for sva, raw, size in old.code_sections():
        a = np.frombuffer(old.data, np.uint8, size, raw)
        pos = np.nonzero(a[:-4] == 0xE9)[0]
        rel = sum(a[pos + 1 + k].astype(np.int64) << (8 * k) for k in range(4))
        rel -= (rel >= 1 << 31) * (1 << 32)
        va = sva + pos
        i = np.searchsorted(begins, va, 'right') - 1
        outside = (i < 0) | (va >= ends[np.maximum(i, 0)])  # stubs are not part of any function
        for p, t in zip(va[outside].tolist(), (va + 5 + rel)[outside].tolist()):
            if not sva <= t < sva + size and t not in orig and old.is_code(t):
                orig[t] = p
    return orig


def lis(pairs):
    """longest subsequence of (x, y) pairs sorted by x with increasing y"""
    tails, idx, prev = [], [], [None] * len(pairs)
    for i, (_, y) in enumerate(pairs):
        p = bisect.bisect_left(tails, y)
        tails[p:p + 1], idx[p:p + 1] = [y], [i]
        prev[i] = idx[p - 1] if p else None
    out, i = [], idx[-1] if idx else None
    while i is not None:
        out.append(pairs[i])
        i = prev[i]
    return out[::-1]


class Layout:
    def __init__(self, old, new):
        self.old, self.new = old, new
        self.ofuncs, self.nfuncs = owners(old), owners(new)
        self.orig = original_addresses(old)
        self.nstarts = sorted(self.nfuncs)
        self.old_ptrs = self.new_ptrs = None

        def key(pe, f):
            o = pe.off(f)
            b = pe.data[o:o + 32]
            return bytes(0 if w else c for c, w in zip(b, mask(b)))

        ok = {f: key(old, f) for f in self.ofuncs}
        nk = {f: key(new, f) for f in self.nfuncs}
        oc, nc = Counter(ok.values()), Counter(nk.values())
        nrev = {k: f for f, k in nk.items() if nc[k] == 1}
        pairs = lis(sorted((self.orig.get(f, f), nrev[k]) for f, k in ok.items() if oc[k] == 1 and k in nrev))
        self.points = dict(pairs)
        self.keys = sorted(self.points)
        print(f'layout: {len(self.ofuncs)} old / {len(self.nfuncs)} new functions, {len(pairs)} anchors')

    def candidates(self, orig, exclude=()):
        i = bisect.bisect_left(self.keys, orig)
        l, r = i - 1, i
        while l >= 0 and self.keys[l] in exclude:
            l -= 1
        while r < len(self.keys) and self.keys[r] in exclude:
            r += 1
        return list(dict.fromkeys(orig + self.points[self.keys[k]] - self.keys[k]
                                  for k in (l, r) if 0 <= k < len(self.keys)))

    def calls(self, pe, funcs, f):
        out = set()
        for b, e in funcs.get(f, []):
            d = pe.data[pe.off(b):pe.off(b) + e - b]
            i = d.find(b'\xe8')
            while 0 <= i < len(d) - 4:
                out.add(b + i + 5 + struct.unpack_from('<i', d, i + 1)[0])
                i = d.find(b'\xe8', i + 1)
        return out

    def code(self, pe, va, leaf):
        """masked bytes at va; leaf functions are cut after the first ret (protector junk follows)"""
        o = pe.off(va)
        b = pe.data[o:o + 48]
        if leaf and 0xC3 in b:
            b = b[:b.index(0xC3) + 1]
        return b, mask(b)

    def similarity(self, f, leaf, c):
        b, m = self.code(self.old, f, leaf)
        o = self.new.off(c)
        if o is None:
            return 0
        n = self.new.data[o:o + len(b)]
        keep = [i for i in range(len(b)) if not m[i]]
        return sum(b[i] == n[i] for i in keep if i < len(n)) / max(len(keep), 1)

    def translated_calls(self, f):
        res = set()
        for t in self.calls(self.old, self.ofuncs, f):
            c = self.candidates(self.orig.get(t, t))
            if len(c) == 1:
                res.add(c[0])
        return res

    def call_share(self, old_calls, c):
        return len(old_calls & self.calls(self.new, self.nfuncs, c)) / len(old_calls) if len(old_calls) >= 3 else None

    def port(self, f, deep=True):
        """new address of old function f (with or without unwind info), or None with reason"""
        orig = self.orig.get(f, f)
        if orig in self.points:
            return self.points[orig], 'layout anchor'
        cands = self.candidates(orig)
        start = f in self.ofuncs
        old_calls = self.translated_calls(f) if start else set()

        if start:
            starts = [c for c in cands if c in self.nfuncs]
            if len(cands) == 1 and starts:
                return starts[0], 'layout'
            if len(starts) == 1:
                share = self.call_share(old_calls, starts[0])
                if share is None:
                    return starts[0], 'layout, unconfirmed'
                if share >= CALLS_MIN:
                    return starts[0], f'layout, calls {share:.0%}'

        # score every function start (leaf: every 16 byte boundary) around the predictions
        lo, hi = min(cands) - WINDOW, max(cands) + WINDOW
        pool = self.nstarts[bisect.bisect_left(self.nstarts, lo):bisect.bisect_right(self.nstarts, hi)]
        if not start:
            pool = sorted(set(pool) | set(range(lo & ~15, hi, 16)))
        scored = []
        for c in pool:
            sim = self.similarity(f, not start, c)
            share = self.call_share(old_calls, c) if start and c in self.nfuncs else None
            scored.append((max(sim, share or 0), sim, share, c))
        scored.sort(reverse=True)
        if not scored:
            return self.port_callers(f, 'layout: no candidates') if deep else (None, 'layout: no candidates')
        best, sim, share, c = scored[0]
        tied = [x for x in scored if x[0] == best]
        if len(tied) > 1:  # identical candidates: only the predicted one is acceptable
            hits = [x[3] for x in tied if x[3] in cands]
            if best >= WINDOW_MIN and len(hits) == 1:
                return hits[0], f'layout window {best:.0%}, predicted of {len(tied)}'
            return self.port_callers(f, f'layout window: {len(tied)} candidates at {best:.0%}') if deep else (None, f'layout window: {len(tied)} candidates at {best:.0%}')
        second = scored[1][0] if len(scored) > 1 else 0
        if best >= WINDOW_MIN and best - second >= FUZZY_GAP:
            return c, f'layout window {best:.0%}/{second:.0%}'
        return self.port_callers(f, f'layout window: best {best:.0%}, second {second:.0%}') if deep else (None, f'layout window: best {best:.0%}, second {second:.0%}')

    def is_start(self, va):
        o = self.new.off(va)
        return va in self.nfuncs or o is not None and va % 16 == 0 and self.new.data[o - 1] in (0xCC, 0xC3, 0x90)

    def port_callers(self, f, why):
        """plain callers still call the stub at the original address: port a call site, follow its target"""
        res, how = port_by_xref(self.old, self.new, self.orig.get(f, f))
        if res is None:
            return self.port_tables(f, f'{why}; callers: {how}')
        if self.is_start(res):
            return res, f'callers {how}'
        return None, f'{why}; callers {how} -> {res:#x} is not a function start'

    def map_pointer(self, v):
        """new address of an old code pointer if it is unambiguous"""
        if not self.old.is_code(v):
            return None
        o = self.old.off(v)
        if self.old.data[o] == 0xE9:  # protector stub: map the function it jumps to
            v += 5 + struct.unpack_from('<i', self.old.data, o + 1)[0]
        res, _ = self.port(v, deep=False)
        return res

    def port_tables(self, f, why):
        """vtables/function tables point at the stub: locate the table in new by its mapped neighbour slots"""
        if self.old_ptrs is None:
            self.old_ptrs = np.frombuffer(self.old.data[:len(self.old.data) // 8 * 8], '<u8')
            self.new_ptrs = np.frombuffer(self.new.data[:len(self.new.data) // 8 * 8], '<u8')
        votes = Counter()
        for r in (np.nonzero(self.old_ptrs == self.orig.get(f, f))[0][:8]).tolist():
            for k in (-3, -2, -1, 1, 2, 3):
                if not 0 <= r + k < len(self.old_ptrs):
                    continue
                mv = self.map_pointer(int(self.old_ptrs[r + k]))
                if mv is None:
                    continue
                for pos in np.nonzero(self.new_ptrs == mv)[0][:64].tolist():
                    if 0 <= pos - k < len(self.new_ptrs):
                        votes[int(self.new_ptrs[pos - k])] += 1
        votes = Counter({v: n for v, n in votes.items() if self.is_start(v)})
        if not votes:
            return None, f'{why}; tables: nothing'
        (best, n), *rest = votes.most_common(2)
        if n >= TABLE_VOTES and (not rest or n >= 2 * rest[0][1]):
            return best, f'tables x{n}' + (f'/{rest[0][1]}' if rest else '')
        return None, f'{why}; tables: weak {n}' + (f'/{rest[0][1]}' if rest else '')

    def mnemonics(self, pe, funcs, f):
        from capstone import CS_ARCH_X86, CS_MODE_64, Cs
        o = pe.off(f)
        ranges = funcs.get(f) or [(f, f + 0x200)]
        return [i.mnemonic for b, e in ranges
                for i in Cs(CS_ARCH_X86, CS_MODE_64).disasm(pe.data[pe.off(b):pe.off(b) + e - b], b)] if o else []

    def resemblance(self, a, b):
        """instruction similarity of the common length; old protected functions carry junk after their code"""
        k = min(len(a), len(b), VERIFY_LEN)
        return difflib.SequenceMatcher(None, a[:k], b[:k], autojunk=False).ratio() if k else 0

    def recheck(self, va, res, alternatives):
        """final check of a function start by its instructions; the new exe also moved some functions behind
        `jmp` stubs, which misleads the layout phase. Returns (address, note)"""
        try:
            old = self.mnemonics(self.old, self.ofuncs, va)
        except ImportError:
            return res, ''
        cands = set(alternatives) | set(self.candidates(self.orig.get(va, va))) | {res}
        for c in list(cands):  # follow stubs
            o = c and self.new.off(c)
            if o and self.new.data[o] == 0xE9:
                cands.add(c + 5 + struct.unpack_from('<i', self.new.data, o + 1)[0])
        scores = {c: self.resemblance(old, self.mnemonics(self.new, self.nfuncs, c)) for c in cands if c and self.new.is_code(c)}
        cur = scores.get(res, 0)
        best = max(scores, key=scores.get, default=None)
        if best is not None and best != res and scores[best] >= VERIFY_GOOD and cur < VERIFY_BAD:
            return best, f', instructions {scores[best]:.0%} replace {res and hex(res)} ({cur:.0%})'
        return res, f', instructions {cur:.0%}'

    def port_inside(self, va, funcs, expected=None):
        """address inside a function (patch site): map the function, then find surrounding bytes in it;
        if the old surroundings were rewritten, find the expected instruction bytes themselves"""
        owner = self.old.owner(funcs, va)
        new_owner, how = self.port(owner)
        if new_owner is None:
            return None, f'owner {owner:#x}: {how}'
        for before, after in ((16, 16), (0, 16), (16, 4), (8, 8)):
            o = self.old.off(va)
            b = self.old.data[o - before:o + after]
            p = pattern(b, mask(b))
            hits = [h.start() + before for b2, e2 in self.nfuncs.get(new_owner, [])
                    for h in p.finditer(self.new.data, self.new.off(b2), self.new.off(e2))]
            if len(hits) == 1:
                return self.new.va(hits[0]), f'inside {owner:#x} ({how}), context -{before}/+{after}'
        if expected:
            def occurrences(pe, ranges):
                res = []
                for b2, e2 in ranges:
                    d = pe.data[pe.off(b2):pe.off(e2)]
                    i = d.find(expected)
                    while i >= 0:
                        res.append(b2 + i)
                        i = d.find(expected, i + 1)
                return sorted(res)

            old_hits = occurrences(self.old, self.ofuncs[owner])
            new_hits = occurrences(self.new, self.nfuncs.get(new_owner, []))
            if len(new_hits) == 1:
                return new_hits[0], f'inside {owner:#x} ({how}), only expected bytes'
            if va in old_hits and len(new_hits) == len(old_hits) > 1:
                return new_hits[old_hits.index(va)], f'inside {owner:#x} ({how}), expected bytes {old_hits.index(va) + 1}/{len(old_hits)}'
        res, why = self.port_by_instructions(va, owner, new_owner)
        if res is not None:
            return res, f'inside {owner:#x} ({how}), {why}'
        return None, f'inside {owner:#x} ({how}): {why}'

    def port_by_instructions(self, va, owner, new_owner):
        """same instruction sequence in both functions (operands' numbers ignored): map by instruction index"""
        try:
            from capstone import CS_ARCH_X86, CS_MODE_64, Cs
        except ImportError:
            return None, 'context not found (install capstone for instruction matching)'
        md = Cs(CS_ARCH_X86, CS_MODE_64)

        def stream(pe, ranges):
            return [(i.address, i.size, i.mnemonic, re.sub(r'([+-] )?0x[0-9a-f]+', 'N', i.op_str))
                    for b, e in ranges for i in md.disasm(pe.data[pe.off(b):pe.off(b) + e - b], b)]

        old_s, new_s = stream(self.old, self.ofuncs[owner]), stream(self.new, self.nfuncs.get(new_owner, []))
        if [x[2:] for x in old_s] != [x[2:] for x in new_s]:
            return None, 'instructions differ'
        k = next((i for i, x in enumerate(old_s) if x[0] == va), None)
        if k is None:
            return None, 'not an instruction boundary'
        if old_s[k][1] != new_s[k][1]:
            return None, 'instruction size differs'
        return new_s[k][0], f'same instructions, #{k}'


def parse_patches():
    """patch address -> expected original bytes, from dynamitePatches.cpp"""
    src = re.sub(r'//[^\n]*', '', PATCHES.read_text())
    return {int(a, 16): bytes(int(x, 16) for x in e.split(',') if x.strip())
            for a, e in re.findall(r'\.address = (0x[0-9a-f]+),\s*\.expected\s*=\s*\{([^}]*)\}', src)}


def signatures(old, new, items):
    """phase 1, cached in CACHE for the same pair of builds"""
    tag = f'{old.timestamp:#x}-{new.timestamp:#x}'
    cache = json.loads(CACHE.read_text()) if CACHE.exists() else {}
    if cache.get('builds') == tag and all(hex(va) in cache['results'] for va in items):
        return {va: tuple(cache['results'][hex(va)]) for va in items}

    results, delta = {}, None
    for i, va in enumerate(items):
        if old.is_code(va):
            res, how = port_code(old, new, va, delta)
            if res is not None:
                delta = res - va
        else:
            res, how = port_data(old, new, va)
        print(f'[{i + 1}/{len(items)}] {va:#x} -> {res and hex(res)} ({how})', flush=True)
        results[va] = (res, how)
    CACHE.write_text(json.dumps({'builds': tag, 'results': {hex(k): v for k, v in results.items()}}))
    return results


def main(old_path, new_path, version, old_dump=None):
    old, new = PE(old_path, old_dump), PE(new_path)
    print(f'old timestamp {old.timestamp:#x}, new timestamp {new.timestamp:#x}')

    table = re.findall(r'^\s+\{"(\w+)",\s*(0x[0-9a-fA-F]+)\},', TABLE.read_text(), re.M)  # commented out entries skipped
    expected = parse_patches()
    patches = [int(a, 16) for a in re.findall(r'^\s+\.address = (0x[0-9a-f]+),', PATCHES.read_text(), re.M)]

    items = sorted({int(a, 16) for _, a in table} | set(patches))
    phase1 = signatures(old, new, items)
    layout = Layout(old, new)

    funcs = old.functions()
    ported, failed = {}, []
    for va in items:
        res, how = phase1[va]
        if old.is_code(va):
            owner = old.owner(funcs, va)
            if owner is not None and owner != va:
                res2, how2 = layout.port_inside(va, funcs, expected.get(va))
            else:
                res2, how2 = layout.port(va)
                if va in layout.ofuncs and res is not None and res not in layout.nfuncs:
                    res, how = None, f'{how} rejected: not a function start'
            phase1_res = res
            if res2 is not None and res is not None and res2 != res:
                how2 += f', overrides {how} {res:#x}'
            if res2 is not None or res is None:
                res, how = res2, how2 if res2 is not None else f'{how}; {how2}'
            if res is not None and (owner is None or owner == va):
                res, note = layout.recheck(va, res, [phase1_res])
                how += note
        print(f'{va:#x} -> {res and hex(res)} ({how})')
        if res is None:
            failed.append(va)
        else:
            ported[va] = res

    lines = ['#ifndef HOOK_DYNAMITE_MGSVTPP_ADDRESSES', '#define HOOK_DYNAMITE_MGSVTPP_ADDRESSES', '',
             '#include <cstdint>', '#include <map>', '#include <string>', '',
             '// do _not_ hook lua_newstate, it will crash no matter what if compiled with mingw', '',
             '// generated by misc/port_addresses.py, FAILED entries could not be ported (old addresses kept)',
             'namespace Dynamite {', f'    constexpr uint32_t exeTimestamp = {new.timestamp:#x};',
             f'    constexpr auto exeVersion = "{version} (EN)";', '',
             '    inline std::map<std::string, uint64_t> mgsvtppAddresses{']
    for name, a in table:
        n = ported.get(int(a, 16))
        lines.append(f'        {{"{name}", {n:#x}}},' if n else f'        // FAILED {{"{name}", {a}}},')
    lines += ['    };', '}', '', '#endif // HOOK_DYNAMITE_MGSVTPP_ADDRESSES']
    OUT.write_text('\n'.join(lines) + '\n')

    print('\npatch sites (bytes at the new address vs dynamitePatches.cpp expected):')
    for va in patches:
        n, exp = ported.get(va), expected.get(va, b'')
        now = new.data[new.off(n):new.off(n) + len(exp)] if n else b''
        state = '' if not n or not exp else 'same bytes' if now == exp else f'NEW BYTES {now.hex()} (was {exp.hex()})'
        print(f'  {va:#x} -> {n and hex(n)} {state}')

    print(f'\n{len(ported)}/{len(items)} ported, failed: {[hex(v) for v in failed]}')
    print(f'table written to {OUT}')


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    main(*sys.argv[1:])
