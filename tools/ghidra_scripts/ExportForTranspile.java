// ExportForTranspile.java - decompile every function for tools/transpile.py.
//
// Writes one file per function into <outDir>/<addr>.c:
//   //@F <addr> <size> <name>
//   //@P <prototype>
//   //@G <name> <addr> <size> <type>        global symbol as typed in this function
//   //@L <name> <stackOffset> <size> <type> stack-resident local (offset from entry sp)
//   <C code>
// and <outDir>/functions.tsv: addr, name, prototype, thunk target, for every function.
//
//   analyzeHeadless ghidra MediEvil -process main.prx -noanalysis \
//       -scriptPath tools/ghidra_scripts -postScript ExportForTranspile.java decomp/transpile_in
//@category MediEvil
import java.io.File;
import java.io.PrintWriter;
import java.util.Iterator;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighCodeSymbol;
import ghidra.program.model.listing.VariableStorage;

public class ExportForTranspile extends GhidraScript {

    @Override
    public void run() throws Exception {
        File outDir = new File(getScriptArgs()[0]);
        outDir.mkdirs();
        DecompInterface decomp = new DecompInterface();
        decomp.setOptions(new DecompileOptions());
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.setSimplificationStyle("decompile");
        decomp.openProgram(currentProgram);

        try (PrintWriter idx = new PrintWriter(new File(outDir, "functions.tsv"), "UTF-8")) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            int n = 0;
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                String addr = f.getEntryPoint().toString();
                Function thunk = f.isThunk() ? f.getThunkedFunction(true) : null;
                idx.println(addr + "\t" + f.getName() + "\t" + f.getSignature().getPrototypeString()
                        + "\t" + (thunk != null ? thunk.getName() : ""));
                if (f.isExternal() || f.isThunk()) continue;

                DecompileResults res = decomp.decompileFunction(f, 60, monitor);
                if (res == null || !res.decompileCompleted() || res.getDecompiledFunction() == null) {
                    println("[tx] FAILED " + addr);
                    continue;
                }
                HighFunction hf = res.getHighFunction();
                try (PrintWriter w = new PrintWriter(new File(outDir, addr + ".c"), "UTF-8")) {
                    w.println("//@F " + addr + " " + f.getBody().getNumAddresses() + " " + f.getName());
                    w.println("//@P " + f.getSignature().getPrototypeString());
                    if (hf != null) {
                        Iterator<HighSymbol> gs = hf.getGlobalSymbolMap().getSymbols();
                        while (gs.hasNext()) {
                            HighSymbol s = gs.next();
                            String a = (s instanceof HighCodeSymbol && ((HighCodeSymbol) s).getStorage() != null
                                    && ((HighCodeSymbol) s).getStorage().getMinAddress() != null)
                                    ? ((HighCodeSymbol) s).getStorage().getMinAddress().toString() : "?";
                            w.println("//@G " + s.getName() + " " + a + " " + s.getSize() + " "
                                    + s.getDataType().getDisplayName());
                        }
                        Iterator<HighSymbol> ls = hf.getLocalSymbolMap().getSymbols();
                        while (ls.hasNext()) {
                            HighSymbol s = ls.next();
                            VariableStorage st = s.getStorage();
                            if (st != null && st.isStackStorage())
                                w.println("//@L " + s.getName() + " " + st.getStackOffset() + " " + s.getSize()
                                        + " " + s.getDataType().getDisplayName());
                        }
                    }
                    w.print(res.getDecompiledFunction().getC());
                }
                if (++n % 500 == 0) println("[tx] " + n);
            }
        }
        decomp.dispose();
        println("[tx] done");
    }
}
