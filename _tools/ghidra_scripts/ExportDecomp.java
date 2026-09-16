// Decompile every function to one C file and write a function index.
// args: <out.c> [timeout_seconds]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.PrintWriter;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outC = args[0];
        int timeout = args.length > 1 ? Integer.parseInt(args[1]) : 90;
        String outIdx = outC.replaceAll("\\.c$", ".functions.csv");

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        int n = 0, failed = 0;
        try (PrintWriter idx = new PrintWriter(outIdx, "UTF-8");
             PrintWriter out = new PrintWriter(outC, "UTF-8")) {
            idx.println("entry,size,name,n_callers,n_callees,decompiled");
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                if (f.isThunk()) continue;
                Address ep = f.getEntryPoint();
                int ncallers = 0, ncallees = 0;
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(ep);
                while (ri.hasNext()) { Reference r = ri.next(); if (r.getReferenceType().isCall()) ncallers++; }
                for (Function c : f.getCalledFunctions(monitor)) ncallees++;
                DecompileResults r = decomp.decompileFunction(f, timeout, monitor);
                out.printf("%n// ==== %s  %s  size=%d  callers=%d callees=%d ====%n",
                        f.getName(), ep, f.getBody().getNumAddresses(), ncallers, ncallees);
                boolean ok = r != null && r.decompileCompleted();
                if (ok) {
                    out.println(r.getDecompiledFunction().getC());
                    n++;
                } else {
                    out.println("// decompile failed: " + (r == null ? "null" : r.getErrorMessage()));
                    failed++;
                }
                idx.printf("%s,%d,%s,%d,%d,%s%n", ep, f.getBody().getNumAddresses(),
                        f.getName(), ncallers, ncallees, ok ? "ok" : "failed");
            }
        }
        decomp.dispose();
        println("decompiled " + n + " functions, " + failed + " failed -> " + outC);
    }
}
