// Export strings (with xrefs), imports and a summary symbol dump to CSV files.
// args: <outDir>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

import java.io.PrintWriter;

public class ExportMeta extends GhidraScript {
    @Override
    public void run() throws Exception {
        String dir = getScriptArgs()[0];
        if (!dir.endsWith("/")) dir += "/";
        SymbolTable st = currentProgram.getSymbolTable();

        // strings + xrefs
        int ns = 0;
        try (PrintWriter w = new PrintWriter(dir + "prage.strings.csv", "UTF-8")) {
            w.println("address,length,string,n_xrefs,xref_from");
            DataIterator di = currentProgram.getListing().getDefinedData(true);
            while (di.hasNext() && !monitor.isCancelled()) {
                Data d = di.next();
                if (d == null || d.getValue() == null) continue;
                DataType dt = d.getDataType();
                if (dt == null || !dt.getName().contains("string")) continue;
                String s = d.getValue().toString().replace("\"", "'").replace("\n", "\\n").replace("\r", "");
                StringBuilder refs = new StringBuilder();
                int n = 0;
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(d.getAddress());
                while (ri.hasNext()) {
                    Reference r = ri.next();
                    if (n++ < 8) { if (refs.length() > 0) refs.append(' '); refs.append(r.getFromAddress()); }
                }
                w.printf("%s,%d,\"%s\",%d,%s%n", d.getAddress(), d.getLength(), s, n, refs);
                ns++;
            }
        }

        // imports
        int ni = 0;
        try (PrintWriter w = new PrintWriter(dir + "prage.imports.csv", "UTF-8")) {
            w.println("address,name,namespace");
            SymbolIterator it = st.getSymbolIterator(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Symbol s = it.next();
                if (s.isExternalEntryPoint()) continue;
                if (s.getSymbolType().toString().contains("External") ||
                        s.getSource() == SourceType.IMPORTED) {
                    w.printf("%s,%s,%s%n", s.getAddress(), s.getName(), s.getParentNamespace().getName());
                    ni++;
                }
            }
        }

        // user-defined + named symbols
        int nn = 0;
        try (PrintWriter w = new PrintWriter(dir + "prage.symbols.csv", "UTF-8")) {
            w.println("address,type,name,namespace,source");
            SymbolIterator it = st.getAllSymbols(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Symbol s = it.next();
                if (s.getSource() != SourceType.USER_DEFINED && s.getSource() != SourceType.IMPORTED) continue;
                w.printf("%s,%s,%s,%s,%s%n", s.getAddress(), s.getSymbolType(), s.getName(),
                        s.getParentNamespace().getName(), s.getSource());
                nn++;
            }
        }
        println("exported strings=" + ns + " imports=" + ni + " symbols=" + nn + " to " + dir);
    }
}
