"""Compare native incremental UTF-8 output with the fixed Pi StreamDecoder."""
from pathlib import Path
import argparse
import hashlib
import random
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'references/pi/packages/durable/src/env/decode.ts'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True)
    args = parser.parse_args()
    # Keep behavior independent from future reference checkout updates.
    expected = '3db30eae71f612b015b95e83b28d64b5330498a517119b5a01e210666cb5b032'
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != expected:
        raise SystemExit('fixed Pi decode.ts changed; review before updating oracle')
    cases = []
    for a in range(256):
        cases.append((bytes([a]), [1]))
        for b in range(256):
            cases.append((bytes([a, b]), [1, 1]))
            cases.append((bytes([a, b]), [2]))
    data = [b'\xe2\x82\xef\xbb\xbf\x61', b'', b'\xef\xbb\xbf\xef\xbb\xbf', '中🙂abc'.encode(),
            b'\xef\xbb\xbf\xe4\xb8', b'\xe0\x80\xaf', b'\xed\xa0\x80',
            b'\xf4\x90\x80\x80', b'\xf0\x90\x80\x80', b'\xff'*4096]
    for first in [0xe0,0xe1,0xed,0xef,0xf0,0xf1,0xf4]:
        for second in [0x7f,0x80,0x8f,0x90,0x9f,0xa0,0xbf,0xc0]:
            data.append(bytes([first,second,0x80,0x80]))
    rng = random.Random(20261025)
    data += [rng.randbytes(rng.randrange(1,97)) for _ in range(1000)]
    for raw in data:
        for cut in range(min(len(raw)+1,8)):
            cases.append((raw, [cut,len(raw)-cut]))
        cases.append((raw,[1]*len(raw)))
    payload = ''.join(raw.hex()+'|'+','.join(map(str,chunks))+'\n' for raw,chunks in cases)
    native = subprocess.run([args.binary,'--oracle'],input=payload,text=True,capture_output=True,check=True,timeout=30)
    oracle = r'''
import {StreamDecoder} from SOURCE;
import {readFileSync} from 'node:fs';
const rows=readFileSync(0,'utf8').trimEnd().split('\n');
let output=[];
for(const row of rows){const [hex,parts]=row.split('|');const raw=Buffer.from(hex,'hex');const d=new StreamDecoder();let pos=0,result=[];
for(const n of parts.split(',').filter(Boolean).map(Number)){result.push(Buffer.from(d.decode(raw.subarray(pos,pos+n))).toString('hex'));pos+=n;}
result.push(Buffer.from(d.decode()).toString('hex'));output.push(result.join(','));}
process.stdout.write(output.join('\n')+'\n');
'''.replace('SOURCE',repr(SOURCE.as_uri()))
    upstream = subprocess.run(['node','--input-type=module','-e',oracle],input=payload,text=True,capture_output=True,check=True,timeout=30)
    actual, expected_rows = native.stdout.splitlines(), upstream.stdout.splitlines()
    assert len(actual)==len(cases)==len(expected_rows)
    for i,(left,right) in enumerate(zip(actual,expected_rows)):
        if left != right:
            raise AssertionError(f'case {i}: {cases[i]!r}: native={left!r}, Pi={right!r}')
    print(f'PASS: {len(cases)} chunk-by-chunk comparisons with actual fixed Pi StreamDecoder')

if __name__ == '__main__':
    main()
