// Apply C prototypes to functions by name: lines "name<TAB>prototype" ('#' comments).
//   -postScript ApplyPrototypes.java docs/names_functions/psp_prototypes.tsv
//@category MediEvil
import java.nio.file.*;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ApplyPrototypes extends GhidraScript {
    @Override
    public void run() throws Exception {
        int ok = 0, missing = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            if (line.isBlank() || line.startsWith("#")) continue;
            String[] p = line.split("\t");
            Function f = null;
            for (Symbol s : currentProgram.getSymbolTable().getSymbols(p[0])) {
                f = getFunctionAt(s.getAddress());
                if (f != null) break;
            }
            if (f == null) { missing++; continue; }
            FunctionDefinitionDataType def = CParserUtils.parseSignature(null, currentProgram, p[1] + ";", false);
            if (def == null) { println("cannot parse " + p[1]); bad++; continue; }
            if (new ApplyFunctionSignatureCmd(f.getEntryPoint(), def, SourceType.USER_DEFINED).applyTo(currentProgram)) ok++;
            else bad++;
        }
        println("ApplyPrototypes: applied " + ok + ", missing " + missing + ", failed " + bad);
    }
}
