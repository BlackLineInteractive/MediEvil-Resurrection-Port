// Decompile functions containing the given addresses (and optionally their callees) to stdout file.
// args: <outfile> <depth> <addr>...
//@category MediEvil
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        int depth = Integer.parseInt(a[1]);
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        Set<Function> seen = new LinkedHashSet<>();
        Deque<Object[]> q = new ArrayDeque<>();
        for (int i = 2; i < a.length; i++) {
            Function f = getFunctionContaining(toAddr(a[i]));
            if (f != null) { q.add(new Object[]{f, 0}); continue; }
            // data address: decompile every function referencing it
            for (ghidra.program.model.symbol.Reference r : getReferencesTo(toAddr(a[i]))) {
                Function g = getFunctionContaining(r.getFromAddress());
                if (g != null) q.add(new Object[]{g, 0});
            }
        }
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            while (!q.isEmpty()) {
                Object[] e = q.poll();
                Function f = (Function) e[0];
                int lvl = (Integer) e[1];
                if (!seen.add(f) || f.isThunk()) continue;
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                w.println("// ===== " + f.getEntryPoint() + " " + f.getName() + " (depth " + lvl + ")");
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed");
                if (lvl < depth)
                    for (Function c : f.getCalledFunctions(monitor)) q.add(new Object[]{c, lvl + 1});
            }
        }
    }
}
