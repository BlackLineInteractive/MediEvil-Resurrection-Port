// Dumps functions, imported PSP syscalls and FourCC asset-type references to docs/ghidra/.
//@category MediEvil
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.address.Address;
import java.io.*;
import java.util.*;

public class ExportOverview extends GhidraScript {
    static final String[] TYPES = {"GROU","ANIM","PSM","INST","SKEL","ATBL","BNK","MESH","SUBZ","TCD",
        "ENTS","TNV","VOL","LTR","NLOC","RLOC","BIN","LOC","SPL","FLIP","LAND","SNET","CCV","LIT","NLI","TOC0","TOCS"};

    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        out.mkdirs();
        Listing listing = currentProgram.getListing();
        ReferenceManager refs = currentProgram.getReferenceManager();

        try (PrintWriter w = new PrintWriter(new File(out, "functions.tsv"))) {
            w.println("addr\tname\tsize\tcallers");
            for (Function f : listing.getFunctions(true)) {
                w.printf("%s\t%s\t%d\t%d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                    f.getSymbol().getReferenceCount());
            }
        }

        try (PrintWriter w = new PrintWriter(new File(out, "imports.tsv"))) {
            w.println("addr\tname\tcallers");
            for (Function f : listing.getFunctions(true)) {
                Address a = f.getEntryPoint();
                MemoryBlockName: {
                    String blk = currentProgram.getMemory().getBlock(a).getName();
                    if (!blk.contains("sceStub")) break MemoryBlockName;
                    w.printf("%s\t%s\t%d%n", a, f.getName(), f.getSymbol().getReferenceCount());
                }
            }
        }

        // FourCC immediates (little-endian 'ANIM' etc. built via lui/ori or loaded as constants)
        Map<Long, String> cc = new HashMap<>();
        for (String t : TYPES) {
            byte[] b = Arrays.copyOf(t.getBytes(), 4);
            long le = (b[0] & 0xff) | (b[1] & 0xff) << 8 | (b[2] & 0xff) << 16 | (long)(b[3] & 0xff) << 24;
            long be = (b[3] & 0xff) | (b[2] & 0xff) << 8 | (b[1] & 0xff) << 16 | (long)(b[0] & 0xff) << 24;
            cc.put(le, t + "(le)"); cc.put(be, t + "(be)");
            cc.put(le >>> 16, t + "(le hi)"); cc.put(be >>> 16, t + "(be hi)");
        }
        try (PrintWriter w = new PrintWriter(new File(out, "fourcc_refs.tsv"))) {
            w.println("addr\tfunction\tinsn\tmatch");
            for (Instruction ins : listing.getInstructions(true)) {
                for (int i = 0; i < ins.getNumOperands(); i++) {
                    for (Object o : ins.getOpObjects(i)) {
                        if (!(o instanceof Scalar)) continue;
                        long v = ((Scalar) o).getUnsignedValue();
                        String m = cc.get(v);
                        boolean isLui = ins.getMnemonicString().equals("lui");
                        if (m == null || (m.contains("hi") != isLui)) continue;
                        Function f = getFunctionContaining(ins.getAddress());
                        w.printf("%s\t%s\t%s\t%s%n", ins.getAddress(), f == null ? "?" : f.getName(), ins, m);
                    }
                }
            }
        }

        try (PrintWriter w = new PrintWriter(new File(out, "strings.tsv"))) {
            w.println("addr\txrefs\tstring");
            for (Data d : listing.getDefinedData(true)) {
                if (!d.hasStringValue()) continue;
                String s = String.valueOf(d.getValue()).replace("\t", "\\t").replace("\n", "\\n");
                w.printf("%s\t%d\t%s%n", d.getAddress(), refs.getReferenceCountTo(d.getAddress()), s);
            }
        }
        println("Exported to " + out);
    }
}
