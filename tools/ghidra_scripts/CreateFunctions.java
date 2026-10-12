// Disassemble and create functions at addresses listed one per line (hex). Used for virtual methods
// that auto-analysis missed because nothing calls them directly (only vtables point at them).
//   -postScript CreateFunctions.java <addresses.txt>
//@category MediEvil
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.nio.file.*;

public class CreateFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        int made = 0, existed = 0, failed = 0;
        for (String s : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            s = s.trim();
            if (s.isEmpty()) continue;
            Address a = toAddr(Long.parseLong(s, 16));
            if (getFunctionAt(a) != null) { existed++; continue; }
            if (getInstructionAt(a) == null) {
                clearListing(a);   // a stray data definition would block disassembly
                disassemble(a);
            }
            Function f = createFunction(a, null);
            if (f != null) made++; else failed++;
        }
        println("CreateFunctions: created " + made + ", existed " + existed + ", failed " + failed);
    }
}
