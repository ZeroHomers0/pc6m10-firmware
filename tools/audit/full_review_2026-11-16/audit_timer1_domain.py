"""TIMER1全部8位扫描计数域的原BIN/ELF差分。"""
from pathlib import Path
import sys,os,inspect,re
os.chdir(Path(__file__).resolve().parents[3]);sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
src=inspect.getsource(V.verify_timer1_matrix)
src=re.sub(r"for scan in \(.*?\):", "for scan in range(256):", src, count=1, flags=re.S)
src=src.replace('verify_timer1_matrix','verify_timer1_full_domain').replace('TIMER1_MATRIX','TIMER1_FULL_DOMAIN')
ns=dict(V.__dict__);exec(src,ns);ns['verify_timer1_full_domain']()
