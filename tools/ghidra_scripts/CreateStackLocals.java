// Define stack locals where Ghidra only shows "stack0xNNNN" (addresses taken but no variable), so the
// decompiler tracks writes through those pointers. Input lines: "<funcaddr> stack0x<offset>".
//@category MediEvil
import java.nio.file.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.Undefined8DataType;
import ghidra.program.model.data.Undefined4DataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.StackFrame;
import ghidra.program.model.symbol.SourceType;

public class CreateStackLocals extends GhidraScript {
    @Override
    public void run() throws Exception {
        int ok = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            String[] p = line.trim().split(" ");
            if (p.length < 2) continue;
            Function f = getFunctionAt(toAddr(Long.parseLong(p[0], 16)));
            if (f == null) { bad++; continue; }
            int off = (int) Long.parseLong(p[1], 16);
            StackFrame fr = f.getStackFrame();
            if (fr.getVariableContaining(off) != null) continue;
            try {
                String name = (off < 0 ? "lvar_" : "arg_") + Integer.toHexString(Math.abs(off));
                fr.createVariable(name, off, off >= 0 ? Undefined4DataType.dataType : Undefined8DataType.dataType, SourceType.USER_DEFINED);
                ok++;
            } catch (Exception e) {
                try {
                    fr.createVariable("lvar4_" + Integer.toHexString(Math.abs(off)), off, Undefined4DataType.dataType, SourceType.USER_DEFINED);
                    ok++;
                } catch (Exception e2) { println("fail " + line + ": " + e2.getMessage()); bad++; }
            }
        }
        println("CreateStackLocals: created " + ok + ", failed " + bad);
    }
}
