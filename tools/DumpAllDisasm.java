// DumpAllDisasm.java — dump full disassembly (with literal-pool values) for every function
// Output: tools/_disasm/<entry>_<name>.txt  (one file per function)
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpAllDisasm extends GhidraScript {

    String outDir = "D:/code/LPC1765FBD100/decompiled/tools/_disasm/";

    @Override
    public void run() throws Exception {
        java.io.File d = new java.io.File(outDir);
        if (!d.exists()) d.mkdirs();

        FunctionIterator fit = currentProgram.getFunctionManager().getFunctions(true);
        int n = 0;
        while (fit.hasNext()) {
            Function f = fit.next();
            long entry = f.getEntryPoint().getOffset();
            String name = f.getName();
            String path = outDir + String.format("%08x", entry) + "_" + name + ".txt";
            PrintWriter w = new PrintWriter(new FileWriter(path));
            w.println("# " + name + " entry=0x" + String.format("%08x", entry)
                + " body=" + f.getBody().getNumAddresses());

            InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (it.hasNext()) {
                Instruction ins = it.next();
                StringBuilder sb = new StringBuilder();
                sb.append(ins.getAddress()).append("  ").append(ins.getMnemonicString());
                int nop = ins.getNumOperands();
                for (int i = 0; i < nop; i++) {
                    sb.append(i == 0 ? "  " : ", ");
                    sb.append(ins.getDefaultOperandRepresentation(i));
                }
                w.println(sb.toString());

                // resolve memory references -> annotate literal pool / RAM values
                Reference[] refs = ins.getReferencesFrom();
                for (Reference r : refs) {
                    Address to = r.getToAddress();
                    long v = to.getOffset();
                    if (r.getReferenceType().isData()) {
                        try {
                            long val = currentProgram.getMemory().getInt(to) & 0xffffffffL;
                            w.println("      ; ref 0x" + String.format("%08x", v)
                                + " -> 0x" + String.format("%08x", val));
                        } catch (Exception e) {
                            w.println("      ; ref 0x" + String.format("%08x", v) + " (no mem)");
                        }
                    } else if (r.getReferenceType().isCall()) {
                        w.println("      ; call -> 0x" + String.format("%08x", v));
                    } else if (r.getReferenceType().isJump()) {
                        w.println("      ; jump -> 0x" + String.format("%08x", v));
                    }
                }
            }
            w.close();
            n++;
        }
        println("Dumped " + n + " functions to " + outDir);
    }
}
