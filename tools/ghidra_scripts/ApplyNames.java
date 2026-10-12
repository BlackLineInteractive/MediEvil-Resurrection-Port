// Rename functions from a TSV of <hexAddr>\t<name>[\t<comment>]. Creates the function when missing.
// Only renames functions that still carry a default name (FUN_*) unless the 2nd arg is "force",
// so hand-given names in the project are never overwritten by a generated table.
//   -postScript ApplyNames.java <names.tsv> [force]
//@category MediEvil
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.io.*;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        boolean force = a.length > 1 && a[1].equals("force");
        int renamed = 0, skipped = 0, created = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(a[0]))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (line.isEmpty() || line.startsWith("#") || line.startsWith("addr")) continue;
                String[] f = line.split("\t");
                Address at = toAddr(Long.parseLong(f[0], 16));
                Function fn = getFunctionAt(at);
                if (fn == null) { fn = createFunction(at, null); created++; }
                if (fn == null) { skipped++; continue; }
                if (!force && fn.getSymbol().getSource() != SourceType.DEFAULT
                        && !fn.getName().startsWith("FUN_")) { skipped++; continue; }
                fn.setName(f[1], SourceType.IMPORTED);
                if (f.length > 2 && !f[2].isEmpty()) fn.setComment(f[2]);
                renamed++;
            }
        }
        println("ApplyNames: renamed " + renamed + ", created " + created + ", skipped " + skipped);
    }
}
