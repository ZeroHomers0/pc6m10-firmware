"""负向验证：独立入口若被提前停止，必须拒绝继续比较或继续下一拍。"""
import importlib.util
from pathlib import Path
from unicorn import UC_HOOK_CODE
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[0] / 'support'))
from unicorn_harness import load_original, call

def stopped_loader():
    u = load_original()
    u.hook_add(UC_HOOK_CODE, lambda m, a, n, unused: m.emu_stop())
    return u

def reject(fn):
    try:
        fn()
    except AssertionError as ex:
        assert '返回' in str(ex), ex
    else:
        raise AssertionError('提前停止的独立执行入口被错误接受')

def module(name):
    spec = importlib.util.spec_from_file_location(name, HERE / (name + '.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m

def main():
    m = module('test_eeprom_load_config_equivalence')
    reject(lambda: m.run(stopped_loader, m.ORIG_LOAD, m.ORIG_READ, m.ORIG_WRITE, bytearray(256)))
    m = module('test_eeprom_sync_matrix_equivalence')
    reject(lambda: m.execute(stopped_loader, m.ORIG_SYNC, m.ORIG_WRITE, []))
    m = module('test_adc_scan_sequence_equivalence')
    reject(lambda: m.run(stopped_loader, m.ORIG_SCAN, m.ORIG_START, m.ORIG_WAIT, [100], 2))
    reject(lambda: call(stopped_loader(), 0x5ca))
    print('DIRECT_COMPLETION: PASS 配置加载/参数同步/ADC连续拍/公共call拒绝提前停止 4/4')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
