// Commit the decompiler's inferred parameters and return type of every function to the database, so
// call sites are decompiled with the arguments the machine code really passes (Ghidra otherwise keeps
// the auto-analysis prototype, often "void f(void)"). Prototypes set by hand (USER_DEFINED) are kept.
// Run it twice: a callee's committed prototype changes what its callers infer.
//@category MediEvil
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.symbol.SourceType;

public class CommitSignatures extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface d = new DecompInterface();
        d.setOptions(new DecompileOptions());
        d.openProgram(currentProgram);
        int ok = 0, skipped = 0, failed = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.isExternal() || f.isThunk() || f.getSignatureSource() == SourceType.USER_DEFINED
                    || f.getBody().getNumAddresses() <= 8) { skipped++; continue; }
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            HighFunction hf = r == null ? null : r.getHighFunction();
            if (hf == null) { failed++; continue; }
            try {
                HighFunctionDBUtil.commitParamsToDatabase(hf, true,
                        HighFunctionDBUtil.ReturnCommitOption.COMMIT, SourceType.ANALYSIS);
                ok++;
            } catch (Exception e) { failed++; }
            if ((ok + failed) % 2000 == 0) println("[sig] " + (ok + failed));
        }
        d.dispose();
        println("CommitSignatures: committed " + ok + ", skipped " + skipped + ", failed " + failed);
    }
}
