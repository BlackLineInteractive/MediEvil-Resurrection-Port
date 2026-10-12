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
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import ghidra.app.decompiler.ClangNode;
import ghidra.app.decompiler.ClangToken;
import ghidra.app.decompiler.ClangTokenGroup;
import ghidra.app.decompiler.ClangTypeToken;
import ghidra.program.model.data.AbstractFloatDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;

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
        // optional second argument: a file of function addresses to export (the rest are skipped and
        // functions.tsv is not rewritten)
        java.util.Set<String> only = null;
        if (getScriptArgs().length > 1) {
            only = new java.util.HashSet<>();
            for (String l : java.nio.file.Files.readAllLines(java.nio.file.Paths.get(getScriptArgs()[1])))
                if (!l.isBlank()) only.add(l.trim().split("\\s+")[0]);
        }
        DecompInterface decomp = new DecompInterface();
        decomp.setOptions(new DecompileOptions());
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.setSimplificationStyle("decompile");
        decomp.openProgram(currentProgram);

        try (PrintWriter idx = new PrintWriter(new File(outDir, only == null ? "functions.tsv" : "functions.partial.tsv"), "UTF-8")) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            int n = 0;
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                String addr = f.getEntryPoint().toString();
                Function thunk = f.isThunk() ? f.getThunkedFunction(true) : null;
                idx.println(addr + "\t" + f.getName() + "\t" + f.getSignature().getPrototypeString()
                        + "\t" + (thunk != null ? thunk.getName() : ""));
                if (f.isExternal() || f.isThunk()) continue;
                if (only != null && !only.contains(addr)) continue;

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
                    w.print(markedC(res));
                }
                if (++n % 500 == 0) println("[tx] " + n);
            }
        }
        decomp.dispose();
        println("[tx] done");
    }

    private static boolean isFloat(Varnode v) {
        if (v == null || v.getHigh() == null) return false;
        DataType t = v.getHigh().getDataType();
        while (t instanceof TypeDef) t = ((TypeDef) t).getBaseDataType();
        return t instanceof AbstractFloatDataType;
    }

    private static boolean qualifies(PcodeOp op) {
        return op != null && op.getOpcode() == PcodeOp.CAST && op.getOutput() != null
                && isFloat(op.getInput(0)) != isFloat(op.getOutput())
                && op.getInput(0).getSize() == op.getOutput().getSize();
    }

    // The op of a cast is not on its tokens: it is the op of the operand's variable token, or a
    // descendant of the value the operand expression produces.
    private static boolean feedsCast(PcodeOp op) {
        if (op == null) return false;
        if (op.getOpcode() == PcodeOp.CAST) return qualifies(op);
        Varnode out = op.getOutput();
        if (out == null) return false;
        Iterator<PcodeOp> it = out.getDescendants();
        while (it.hasNext()) if (qualifies(it.next())) return true;
        return false;
    }

    private static boolean isBitCast(List<ClangNode> toks, int i) {
        int k = i + 1;
        // closing ')' of the cast
        while (k < toks.size() && ((ClangToken) toks.get(k)).getText().trim().isEmpty()) k++;
        if (k >= toks.size() || !((ClangToken) toks.get(k)).getText().equals(")")) return false;
        k++;
        while (k < toks.size() && ((ClangToken) toks.get(k)).getText().trim().isEmpty()) k++;
        if (k >= toks.size()) return false;
        ClangToken first = (ClangToken) toks.get(k);
        if (!first.getText().equals("(")) {
            if (feedsCast(first.getPcodeOp())) return true;
            // function call or unary operator: look a few tokens on
            for (int m = k + 1, seen = 0; m < toks.size() && seen < 3; m++) {
                ClangToken u = (ClangToken) toks.get(m);
                if (u.getText().trim().isEmpty()) continue;
                seen++;
                if (feedsCast(u.getPcodeOp())) return true;
            }
            return false;
        }
        int depth = 0;
        for (int m = k; m < toks.size(); m++) {
            ClangToken u = (ClangToken) toks.get(m);
            String x = u.getText();
            if (x.equals("(")) depth++;
            else if (x.equals(")")) {
                if (--depth == 0) {
                    // postfix operators after the group: (&DAT_x)[i], (...)->f
                    for (int q = m + 1, seen = 0; q < toks.size() && seen < 2; q++) {
                        ClangToken v = (ClangToken) toks.get(q);
                        if (v.getText().trim().isEmpty()) continue;
                        seen++;
                        if (feedsCast(v.getPcodeOp())) return true;
                    }
                    break;
                }
            }
            else if (depth == 1 && feedsCast(u.getPcodeOp())) return true;
        }
        return false;
    }

    // C text where a cast that only reinterprets bits between float and integer (p-code CAST, not
    // TRUNC / INT2FLOAT) has its type token prefixed with "__bc_", e.g. (__bc_uint)fVar1.
    private static String markedC(DecompileResults res) {
        ClangTokenGroup root = res.getCCodeMarkup();
        if (root == null) return res.getDecompiledFunction().getC();
        List<ClangNode> toks = new ArrayList<>();
        root.flatten(toks);
        StringBuilder sb = new StringBuilder();
        for (ClangNode n : toks) {
            ClangToken t = (ClangToken) n;
            if (t instanceof ghidra.app.decompiler.ClangBreak) {
                sb.append('\n');
                int indent = ((ghidra.app.decompiler.ClangBreak) t).getIndent();
                for (int i = 0; i < indent; i++) sb.append(' ');
                continue;
            }
            String text = t.getText();
            if (t instanceof ClangTypeToken && isBitCast(toks, toks.indexOf(n)))
                text = "__bc_" + text;
            sb.append(text);
        }
        return sb.toString();
    }
}
