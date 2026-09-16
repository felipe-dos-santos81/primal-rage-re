// Export the call graph (caller -> callee) as CSV. args: <out.csv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

import java.io.PrintWriter;

public class ExportCallGraph extends GhidraScript {
    @Override
    public void run() throws Exception {
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0], "UTF-8")) {
            w.println("caller,caller_name,callee,callee_name");
            int n = 0;
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.isThunk()) continue;
                for (Function c : f.getCalledFunctions(monitor)) {
                    w.printf("%s,%s,%s,%s%n", f.getEntryPoint(), f.getName(),
                            c.getEntryPoint(), c.getName());
                    n++;
                }
            }
            println("edges=" + n);
        }
    }
}
