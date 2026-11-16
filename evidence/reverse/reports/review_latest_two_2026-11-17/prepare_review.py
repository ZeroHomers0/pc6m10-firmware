"""从已提交源码重建临时复核输入；不修改正式源码或连接硬件。

从仓库根目录执行本文件，再按各临时 firmware/build.sh 构建。
正式工程应处于被复核版本1a55f84，并完成构建。
"""
import io
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = Path(__file__).resolve().parent
TEMP = ROOT / 'tmp/review_latest_two'
TEMP.mkdir(parents=True, exist_ok=True)

for label, revision in [('before', '94c2bc5'), ('candidate', '1a55f84')]:
    target = TEMP / label
    archive = subprocess.check_output(
        ['git', 'archive', '--format=zip', revision, 'firmware'], cwd=ROOT)
    with zipfile.ZipFile(io.BytesIO(archive)) as source:
        for entry in source.infolist():
            resolved = (target / entry.filename).resolve()
            if not resolved.is_relative_to(target.resolve()):
                raise ValueError('归档路径越界')
        source.extractall(target)

candidate = TEMP / 'candidate/firmware/src/09_output_stage.c'
text = candidate.read_text(encoding='utf-8')
old = ('*output_reference_average_ptr = *parameter_voltage_limit_ptr;\n'
       '            *adc_field_output_scaled_ptr = *adc_field_output_scaled_ptr;')
new = ('*output_reference_average_ptr = *parameter_voltage_limit_ptr;\n'
       '            *adc_field_output_scaled_ptr = *adc_voltage_output_scaled_ptr;')
assert text.count(old) == 3, '候选修改位置与审查版本不一致'
candidate.write_text(text.replace(old, new), encoding='utf-8')

for name in ('review.py', 'details.py', 'essential.py'):
    (TEMP / name).write_bytes((EVIDENCE / name).read_bytes())

script = (EVIDENCE / 'review.py').read_text(encoding='utf-8')
script = script.replace("for label in ('latest','before'):", "for label in ('candidate',):")
script = script.replace("if label=='before':", "if label=='candidate':")
script = script.replace("V.FW=ROOT/'tmp/review_latest_two/before/firmware'",
                        "V.FW=ROOT/'tmp/review_latest_two/candidate/firmware'")
script = script.replace("'tmp/review_latest_two/results.json'",
                        "'tmp/review_latest_two/candidate_results.json'")
(TEMP / 'candidate_review.py').write_text(script, encoding='utf-8')
print('已恢复临时复核源码及脚本；构建before/candidate后可运行review.py和candidate_review.py。')
