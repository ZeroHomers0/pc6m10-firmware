"""Generate the combined PC6M-10/PC12M-2 control-system report.

Requires python-docx. The generated DOCX is written to ``tmp/`` so it can be
reviewed before being copied to the shared documentation directory.
"""

from pathlib import Path
from docx import Document
from docx.shared import Cm, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.section import WD_SECTION
from docx.oxml import OxmlElement
from docx.oxml.ns import qn


OUT = Path(__file__).resolve().parents[2] / "tmp" / "PC6M-10_PC12M-2控制与保护系统程序分析.docx"


def set_font(run, name="宋体", size=10.5, bold=False, color="000000"):
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), name)
    run.font.size = Pt(size)
    run.bold = bold
    run.font.color.rgb = RGBColor.from_string(color)


def shade(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    tc_pr.append(shd)


def margins(cell, top=100, start=120, bottom=100, end=120):
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    tcMar = tcPr.first_child_found_in("w:tcMar")
    if tcMar is None:
        tcMar = OxmlElement("w:tcMar")
        tcPr.append(tcMar)
    for tag, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tcMar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            tcMar.append(node)
        node.set(qn("w:w"), str(value)); node.set(qn("w:type"), "dxa")


def set_repeat_table_header(row):
    trPr = row._tr.get_or_add_trPr()
    tblHeader = OxmlElement("w:tblHeader")
    tblHeader.set(qn("w:val"), "true")
    trPr.append(tblHeader)


def table(doc, headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    t.style = "Table Grid"
    set_repeat_table_header(t.rows[0])
    for i, h in enumerate(headers):
        c = t.rows[0].cells[i]; c.text = h; shade(c, "244A73")
        c.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER; margins(c)
        for r in c.paragraphs[0].runs: set_font(r, "微软雅黑", 9, True, "FFFFFF")
        c.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    for ri, row in enumerate(rows):
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = str(v); cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER; margins(cells[i])
            if ri % 2: shade(cells[i], "F2F6FA")
            for p in cells[i].paragraphs:
                p.paragraph_format.space_after = Pt(0); p.paragraph_format.line_spacing = 1.08
                for r in p.runs: set_font(r, "宋体", 8.6)
                if i == 0 and len(str(v)) < 18: p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    if widths:
        for row in t.rows:
            for i, w in enumerate(widths): row.cells[i].width = Cm(w)
    doc.add_paragraph().paragraph_format.space_after = Pt(1)
    return t


def add_p(doc, text="", bold_lead=None):
    p = doc.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0.74)
    p.paragraph_format.line_spacing = 1.35
    p.paragraph_format.space_after = Pt(5)
    if bold_lead and text.startswith(bold_lead):
        r = p.add_run(bold_lead); set_font(r, bold=True)
        r = p.add_run(text[len(bold_lead):]); set_font(r)
    else:
        r = p.add_run(text); set_font(r)
    return p


def add_flow(doc, lines):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(4); p.paragraph_format.space_after = Pt(8)
    for i, line in enumerate(lines):
        r = p.add_run(line + ("\n↓\n" if i < len(lines)-1 else "")); set_font(r, "等线", 9.5, False, "17365D")


def heading(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    p.paragraph_format.keep_with_next = True
    p.paragraph_format.space_before = Pt(10 if level == 1 else 7)
    p.paragraph_format.space_after = Pt(4)
    for r in p.runs: set_font(r, "微软雅黑", 15 if level == 1 else 12, True)
    return p


def bullets(doc, items):
    for x in items:
        p = doc.add_paragraph(style="List Bullet")
        p.paragraph_format.left_indent = Cm(0.8); p.paragraph_format.space_after = Pt(3); p.paragraph_format.line_spacing = 1.25
        for r in p.runs: set_font(r)
        if not p.runs: set_font(p.add_run(x))
        else: p.runs[0].text = x


def build():
    doc = Document()
    sec = doc.sections[0]
    sec.top_margin = Cm(2.2); sec.bottom_margin = Cm(2.0); sec.left_margin = Cm(2.25); sec.right_margin = Cm(2.05)
    styles = doc.styles
    for name in ("Normal", "Body Text"):
        st = styles[name]; st.font.name = "宋体"; st._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体"); st.font.size = Pt(10.5)
    title = doc.add_paragraph(style="Title"); title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.paragraph_format.space_before = Pt(58); title.paragraph_format.space_after = Pt(18)
    set_font(title.add_run("PC6M 10与PC12M 2控制与保护系统程序分析"), "微软雅黑", 22, True)
    sub = doc.add_paragraph(); sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_font(sub.add_run("三相桥式晶闸管恒压恒流直流电源"), "微软雅黑", 13)
    sub.paragraph_format.space_after = Pt(32)
    meta = doc.add_paragraph(); meta.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_font(meta.add_run("依据原始固件反汇编 可编译复刻源码及执行级等价验证\n2026年9月"), "宋体", 10)
    doc.add_page_break()

    heading(doc, "摘要", 1)
    add_p(doc, "PC6M-10和PC12M-2均以LPC1765为核心，根据三相同步信号、直流电压和电流反馈、外部给定及运行保护状态，计算晶闸管同步过零后的触发延时。控制输出不是PWM电压，而是经TIMER2确定触发角，再由TIMER1生成多路门极脉冲序列。两板的软件架构和恒压恒流控制算法基本同构，主要区别是输出脉冲组织形式和实际板端通道数量。")
    add_p(doc, "程序实现软启动、软停止、恒压恒流限幅切换、分区变增益离散PID、过压、欠压、IF过载、三相同步缺失、急停和外部故障保护。固件没有独立温度采样通道或温度整定参数；整机温度保护若存在，应由外部温控器通过E1外部故障或急停输入接入。")
    heading(doc, "结论边界", 2)
    bullets(doc, [
        "程序已证实的内容：采样通道、主状态机、保护比较对象、故障位、PID公式、同步中断、TIMER2延时、TIMER1脉冲序列和Modbus寄存器映射。",
        "需要实机确认的内容：门极有效电平、各路门极与功率器件编号、保护菜单时间单位的准确秒数、脉冲实际宽度以及带载保护配合。",
        "CT过载阈值和时间可以通过菜单及Modbus设置，但当前执行代码中没有找到独立CT过载跳闸比较路径，不能把该菜单项视为已经投入的主保护。",
    ])

    heading(doc, "1 系统总体架构")
    add_flow(doc, ["三相同步及六路模拟量采集", "输入去抖 参数存储及Modbus通信", "运行状态机和集中故障管理", "软启动 恒压恒流切换及PID", "触发角定时换算", "TIMER2单次延时和TIMER1多路门极脉冲"])
    table(doc, ["层级", "主要功能", "硬件或程序模块"], [
        ["测量层", "采集IA、IB、IC、Ug、IF、Uf并平均、缩放和标定", "ADC0及05_adc.c"],
        ["同步层", "接收三相同步下降沿，判断相位、缺相和工频状态", "EINT1、EINT2、EINT3"],
        ["输入层", "按键、RUN、STOP、急停、外部故障及复位去抖", "03_input_debounce.c"],
        ["控制层", "状态机、软起停、恒压恒流切换、PID和限幅", "07_state_machine.c、09_output_stage.c、12_closed_loop.c"],
        ["触发层", "把控制量换算为同步后的触发延时和门极脉冲序列", "TIMER2、TIMER1及触发GPIO"],
        ["通信存储", "参数、测量、状态和故障远程读写，参数掉电保存", "UART3、RS485、AT24C02"],
    ], [2.0, 7.8, 6.0])
    add_p(doc, "PC6M-10主桥六路触发脚为P0.17、P0.15、P0.18、P2.9、P0.19、P0.16，另有P2.8、P2.7、P2.6、P2.5、P0.8、P0.7六路扩展逻辑。PC12M-2固件使用上述十二个GPIO组织十二路触发。两板均以全部触发脚置高作为程序定义的安全封锁态，但板端最终有效极性仍需结合驱动器和脉冲变压器实测。")

    heading(doc, "2 控制主流程")
    add_flow(doc, ["系统时钟和GPIO安全初始化", "输入 TIMER0 I2C ADC初始化", "从EEPROM装载或恢复默认参数", "触发定时器 同步中断 UART3初始化", "认证和上电联锁检查", "进入周期主循环", "采样 输入 状态机 输出级 通信 喂狗"])
    add_p(doc, "主循环由TIMER0节拍驱动。每一轮先后执行ADC扫描、输入扫描、第二次ADC扫描、界面和运行状态机、输出控制、看门狗喂狗、UART接收超时判断及Modbus分派。输出控制函数内部再按十次调用一次的节流方式执行保护、软启动、闭环和输出限幅。")
    add_p(doc, "运行成立需同时满足无故障、急停和外部联锁允许、启停状态成立、给定量达到程序最低有效门槛以及三相同步和工频状态合法。任一总故障置位后，运行配置、给定、PID和斜坡工作变量均被清零，全部触发通道被封锁。")

    heading(doc, "3 硬件上电自检流程")
    add_p(doc, "固件没有传统意义上的完整上电自检程序，例如逐路门极回读、RAM March测试、传感器开短路检测或功率器件诊断。其上电检查属于初始化检查、参数有效性检查和硬联锁检查。")
    table(doc, ["顺序", "检查或动作", "失败表现"], [
        ["1", "等待PLL0和PLL1锁定并连接系统时钟", "停留在等待循环，不能进入正常运行"],
        ["2", "GPIO方向和安全电平初始化，全部门极输出封锁", "避免上电期间出现随机触发"],
        ["3", "控制电源稳定延时", "为外围电源和显示模块留出稳定时间"],
        ["4", "读取EEPROM参数银行和魔数字段", "魔数异常时按固件默认值整组回写"],
        ["5", "初始化ADC、定时器、同步中断和UART3", "运行期由看门狗和连续诊断发现异常"],
        ["6", "原厂认证或外部模块挑战应答", "原厂可进入锁机画面；当前复刻基线强制放行"],
        ["7", "检查P0.2、P0.3上电硬联锁", "显示联锁错误，保持停机和安全态"],
    ], [1.4, 8.3, 6.1])

    heading(doc, "4 故障检测与保护总流程")
    add_flow(doc, ["同步中断 模拟量采样和开关量去抖", "各保护单元分别累计时间或检查窗口", "超过动作条件后置故障位", "集中故障处理生成显示事件码", "断运行输出 合报警输出", "清运行 PID和软启动状态", "全部触发通道进入安全态", "等待人工或外部复位"])
    table(doc, ["故障位", "主要语义", "主要来源"], [
        ["bit0至bit2", "A、B、C相同步缺失", "三相EINT窗口监视"],
        ["bit3", "IF普通级过载", "输出电流倍数反时限"],
        ["bit4", "输出过电压", "Uf与过压阈值比较"],
        ["bit5", "输出欠电压", "Uf与欠压阈值比较"],
        ["bit9", "IF严重过载", "2.5倍以上快速级"],
        ["bit11", "反馈丢失或过程异常", "反馈过低持续计数"],
        ["bit13", "急停或同步频率异常链", "急停及频率门控"],
        ["bit14", "外部故障输入", "P1.16 E1去抖"],
        ["其他位", "通信、输出、反馈及启动超时等", "相应状态机计数"],
    ], [2.4, 5.6, 7.8])
    add_p(doc, "部分早期反编译注释曾把bit5写成过流，但该位的实际比较对象是Uf和欠压阈值，因此应按欠压故障解释。故障位到人机界面事件码是顺序覆盖映射；多种故障同时存在时，屏幕通常显示最后一个满足映射条件的事件。")

    heading(doc, "5 过电压保护逻辑")
    add_p(doc, "保护主体只在输出模式有效且外部给定Ug大于9时执行。过压阈值为0时保护关闭；当Uf不高于阈值时累计计数立即清零。")
    add_flow(doc, ["Uf高于过压整定值", "过压累计计数加一", "计数大于过压时间乘50", "置bit4过压故障", "集中停机和全脉冲封锁"])
    add_p(doc, "动作判据可写为：Uf大于Uov并持续超过50乘Tov个保护计算周期。实际动作时间等于该计数值乘保护函数实际调用周期，不能在未测量主循环和输出级节拍时简单把菜单数值当作秒数。")

    heading(doc, "6 过电流保护逻辑")
    heading(doc, "6.1 恒压恒流限制", 2)
    add_p(doc, "电流限制首先作为调节层工作。在恒压模式下，电压环负责维持输出电压；当IF达到电流限制后，程序经过约50次或10次状态消抖切换到电流反馈，逐步降低导通程度，使电流稳定在限制值附近。恒流模式则以电流环为主，当电压达到限制时由电压环接管。这是恒压和恒流环的最小输出选择，而不是立即跳闸。")
    heading(doc, "6.2 IF过载反时限", 2)
    table(doc, ["IF与整定值关系", "累计门槛", "动作级别"], [
        ["IF大于等于1.0倍Iset", "50乘Tset", "普通级 bit3"],
        ["IF大于1.5倍Iset", "20乘Tset", "普通级 bit3"],
        ["IF大于2.0倍Iset", "10乘Tset", "普通级 bit3"],
        ["IF大于2.5倍Iset", "5乘Tset", "严重级 bit9"],
        ["IF大于3.0倍Iset", "2乘Tset", "严重级 bit9"],
        ["IF大于3.5倍Iset", "超过1个计数", "严重级 bit9"],
    ], [6.3, 4.0, 5.5])
    add_p(doc, "当IF低于Iset时，过载累计计数清零。倍数越高，动作越快，构成离散反时限特性。")
    heading(doc, "6.3 CT过载菜单项", 2)
    add_p(doc, "CT过载阈值和时间具有完整的EEPROM、菜单和Modbus映射，但在当前两板已经复原的输出保护执行代码中未找到使用它们进行跳闸比较的路径。IA、IB、IC被采样和上传，但不能据此认定固件实现了三相CT过流跳闸。输入侧主保护仍应依赖熔断器、断路器、外部保护继电器，或在经过A/B验证后增加明确的固件逻辑。")

    heading(doc, "7 缺相保护逻辑")
    add_flow(doc, ["EINT1 EINT2 EINT3分别记录三相同步到达", "主状态机每超过100轮形成检查窗", "检查窗内某相没有同步且保护开启", "该相缺失计数加一", "连续5个检查窗后置bit0 bit1或bit2", "集中故障停机和封锁"])
    add_p(doc, "同步恢复后，对应相的缺失计数清零，相关缺相位可被清除。但是整机能否重新运行还取决于其他锁存故障、停机状态和复位流程，程序不会因同步恢复而无条件自动重启。同步频率或相位状态不合法时，触发计算同样被禁止，程序直接保持全部触发脚安全态。")

    heading(doc, "8 温度保护逻辑")
    add_p(doc, "固件没有独立温度ADC通道、温度换算表、温度阈值、温度延时或温度菜单。六个ADC输入已分别确定为IA、IB、IC、Ug、IF和Uf。因此不能把相位限制、IF过载或CT过载参数解释为温度保护。")
    add_flow(doc, ["外部温控器或热继电器测温", "达到动作温度后改变无源触点", "触点接入E1外部故障或急停输入", "程序完成去抖并置总故障", "报警 断运行继电器 封锁全部触发"])
    add_p(doc, "若使用P1.16 E1输入，低电平持续约250个输入扫描周期后置外部故障；若接入P0.6急停链，则按急停方式处理。温度动作值、回差、复归温度和延时均由外部温控装置决定。")

    heading(doc, "9 触发脉冲控制流程")
    add_flow(doc, ["软启动或PID产生控制输出", "内部上下限和相位限制钳位", "换算为out_scale触发时间缩放量", "同步下降沿到达并检查运行 故障 工频和相序", "计算TIMER2 MR0并启动单次延时", "到达触发角后TIMER2中断启动TIMER1", "TIMER1按窗口和步序驱动6路或12路GPIO", "完成本周期后停止并等待下一同步"])
    add_p(doc, "TIMER2匹配值由工频补偿、50或60Hz基准、输出相制式、主从偏移和闭环控制量共同组成。输出要求增加时，控制量通常使同步后的等待时间缩短，即触发角减小，晶闸管平均导通时间增加。")
    add_p(doc, "TIMER1把一个电周期组织为约240个步进和六个60度窗口，在各窗口内交替置位和清零相应触发GPIO，形成高频门极脉冲列及相隔60度的重复触发。PC12M-2的十二路TIMER1执行轨迹已通过972个矩阵用例与原固件比较。故障、停机或同步非法时不继续生成序列，而是立即调用统一安全态函数。")

    heading(doc, "10 PID控制模型及流程")
    add_p(doc, "该控制器采用离散增量式PID并把增量累加成位置输出，同时加入误差分区增益、量程归一化除数、恒压恒流环切换和输出钳位。设当前误差为ek，前两次误差为ek-1和ek-2，程序的增量由三部分构成：二阶差分对应D项，当前误差对应P项，相邻误差差对应I参数项；整体再乘误差区增益并除以量程相关除数。")
    add_p(doc, "程序先计算有符号误差r减y，再按误差绝对值选择高、中、低三档增益。根据电压或电流量程选择8、15、30、42、55、80、100、120、150或180等归一化除数。所得增量加到上次位置输出，再做PID内部上下限钳位和触发相位上下限钳位。负误差必须按有符号数运算；该细节已通过原始BIN与新ELF执行差分确认。")
    table(doc, ["PID配置", "程序行为"], [
        ["低速 中速 快速", "使用预设P和I参数组合"],
        ["自定义", "允许在菜单中编辑P和I，范围1至128"],
        ["D参数", "界面显示为自动，不提供直接编辑入口"],
        ["误差分区", "隐藏参数定义误差上下界和高 中 低三档增益"],
        ["输出", "位置累加后限幅，再转换为同步触发延时"],
    ], [4.2, 11.6])

    heading(doc, "11 保护配合时序和逻辑")
    table(doc, ["优先层", "保护或控制", "作用方式"], [
        ["1", "上电硬联锁 急停 外部故障", "直接禁止运行并封锁全部门极脉冲"],
        ["2", "三相同步缺失或频率非法", "禁止触发定时，达到条件后置同步类故障"],
        ["3", "IF严重过载", "2.5倍以上按5T 2T或近瞬时级动作"],
        ["4", "IF普通级过载", "1.0至2.0倍按50T 20T或10T动作"],
        ["5", "过压定时限", "超过阈值并持续50T后停机"],
        ["6", "欠压 反馈丢失 启动及输出超时", "经状态机计数后停机"],
        ["调节层", "恒压恒流限制和PID", "在跳闸前主动减小导通，限制输出"],
    ], [2.0, 6.0, 7.8])
    add_p(doc, "正常协调关系应为先由恒流或恒压限制环调节，再由IF过载或过压保护跳闸；硬联锁、急停和严重过载则不依赖闭环恢复能力。所有故障最终汇入同一个故障字和统一脉冲封锁出口。")

    heading(doc, "12 通信系统架构")
    add_p(doc, "UART3通过隔离RS485收发链路工作，P0.0为TXD3，P0.1为RXD3，P1.29控制DE和RE，高电平发送、低电平接收。协议为Modbus RTU变体，支持0x03读保持寄存器、0x06写单寄存器和0x10写多个寄存器，采用CRC-16 MODBUS，初值0xFFFF、多项式0xA001，CRC低字节先传输。")
    table(doc, ["寄存器范围", "主要内容"], [
        ["1至12", "运行模式 量程 限制 软起停 相位 控制方式和启动方式"],
        ["13至22", "过压 欠压 IF及CT过载 缺相和三相平衡参数"],
        ["23至32", "PID档位 自定义参数 相位校准及部分只写别名"],
        ["33至40", "运行时间 运行状态 故障字和外部给定"],
        ["41至45", "IA IB IC IF Uf实时测量"],
        ["47至50", "从站地址 波特率 校验方式和通信检测"],
        ["51至55", "五个ADC标定除数"],
        ["56至63", "输入 输出相位 远程输出 起始相位及只读状态"],
    ], [4.0, 11.8])
    add_p(doc, "写单寄存器0x06具有逐项范围检查并执行参数同步。写多个寄存器0x10存在若干落点与写单路径不一致且缺少逐值范围校验，工程应用应优先使用0x06修改保护、PID和标定参数。")

    heading(doc, "13 保护整定计算流程")
    heading(doc, "13.1 测量标定", 2)
    add_p(doc, "保护整定前必须先校准ADC。三相电流通道按互感器比、原始平均值、系数2和各自标定除数换算；IF和Uf分别按电流量程或电压量程乘原始平均值，再除以对应标定除数。标定除数允许范围为3500至4500。")
    add_p(doc, "以标准表值为基准时，新除数等于旧除数乘控制板显示值再除以标准表值。由于显示值与除数成反比，显示偏高应增大除数，显示偏低应减小除数。每次修正后应在20%、50%、80%和额定点复核线性。")
    heading(doc, "13.2 量程和限制", 2)
    bullets(doc, [
        "电压量程按电压传感器和设备允许满量程设置；电流量程按IF通道实际满量程设置；互感器比按三相CT一次额定值设置。",
        "电压限制不得高于电压量程，电流限制不得高于电流量程。",
        "恒压电源应把电流限制设在允许连续电流附近，并保证其低于IF过载跳闸值，以形成先限流后跳闸。",
    ])
    heading(doc, "13.3 保护值和时间", 2)
    table(doc, ["项目", "整定原则", "验证方法"], [
        ["过压", "按负载耐压和工艺上限确定，且不超过电压量程", "注入Uf并测量从越限到门极封锁的时间"],
        ["欠压", "低于正常最低运行电压，并躲过软启动和负载突变", "缓慢降低Uf和阶跃降低Uf分别验证"],
        ["电流限制", "按连续允许电流设置，先于IF过载跳闸", "低压假负载下观察恒压转恒流过程"],
        ["IF过载", "结合SCR 变压器 电抗器及负载短时过载曲线", "按1.0 1.5 2.0 2.5 3.0 3.5倍分点注入"],
        ["缺相", "仅有启用开关，软件延时固定", "依次断开TU TV TW并测量动作时间"],
        ["温度", "在外部温控器中设动作值 回差和复归方式", "加热或触点模拟，验证E1 急停和脉冲封锁"],
        ["CT过载", "当前代码未确认独立动作路径，不作为投入主保护", "由外部保护承担或修改固件后重新验证"],
    ], [2.4, 8.0, 5.4])
    add_p(doc, "保护菜单的时间值最终应通过实测标定为秒。过压和欠压的程序门槛为时间参数乘50个保护计算周期；IF过载再按电流倍数改为50T、20T、10T、5T、2T或近瞬时。保护计算周期受系统时钟、TIMER0和输出级节流影响，不能只凭菜单数字推定。")

    heading(doc, "实施和验证要求")
    bullets(doc, [
        "首次验证仅接限流控制电源，断开市电、门极和功率负载。",
        "先通过信号源注入同步和ADC信号验证显示值、阈值、延时及故障位，再接门极驱动板观察空载脉冲。",
        "任何源代码修改后必须分别以PC6M-10和PC12M-2各自原始BIN进行A/B执行级验证，不能用一个项目的结果代替另一个项目。",
        "带载前确认急停、外部故障、缺相、过压和过流时所有触发通道均进入安全态。",
    ])

    heading(doc, "主要程序证据")
    table(doc, ["文件", "证据内容"], [
        ["PC6M-10 firmware src 01_startup.c", "上电初始化 主循环 认证 联锁 看门狗"],
        ["PC6M-10 firmware src 05_adc.c", "六通道ADC扫描 平均 量程和标定换算"],
        ["PC6M-10 firmware src 07_state_machine.c", "运行 故障 缺相 复位 菜单和继电器处理"],
        ["PC6M-10 firmware src 09_output_stage.c", "软起停 保护 恒压恒流切换 触发角和定时器"],
        ["PC6M-10 firmware src 12_closed_loop.c", "分区变增益离散PID及有符号误差链"],
        ["PC6M-10 docs analysis UART3_PROTOCOL.md", "Modbus物理层 帧格式 寄存器和异常码"],
        ["PC6M-10 docs analysis MENU_PARAMETER_MAPPING.md", "基本 保护 通信 PID和相位菜单映射"],
        ["PC12M-2 firmware src 09_output_stage.c", "十二路触发序列和PC12M-2独立原固件证据"],
        ["PC12M-2 docs analysis HARDWARE_VERIFICATION_2026-08-31.md", "十二路GPIO 同步输入和板级事实边界"],
    ], [6.6, 9.2])

    for section in doc.sections:
        footer = section.footer.paragraphs[0]
        footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
        set_font(footer.add_run("PC6M-10与PC12M-2控制与保护系统程序分析"), "宋体", 8, False, "666666")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUT)
    print(OUT)


if __name__ == "__main__":
    build()
