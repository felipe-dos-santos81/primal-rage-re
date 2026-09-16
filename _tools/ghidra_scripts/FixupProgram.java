// Fix up the loaded LE image: ensure the code object is executable and that the
// LE entry point is disassembled and turned into a function.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

public class FixupProgram extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println(String.format("block %-16s %s-%s x=%b w=%b r=%b",
                    b.getName(), b.getStart(), b.getEnd(), b.isExecute(), b.isWrite(), b.isRead()));
        }
        // Make the code object executable.
        MemoryBlock code = currentProgram.getMemory().getBlock(".object1");
        if (code != null && !code.isExecute()) {
            code.setExecute(true);
            println("set .object1 executable");
        } else if (code == null) {
            // fall back: mark any block holding code addresses
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                if (b.getStart().getOffset() == 0x10000) { b.setExecute(true); println("marked " + b.getName()); }
            }
        }
        for (Address ep : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
            println("entry " + ep);
            if (getInstructionAt(ep) == null) disassemble(ep);
            Function f = getFunctionAt(ep);
            if (f == null) f = createFunction(ep, null);
            println("  -> function " + (f == null ? "FAILED" : f.getName()));
        }
    }
}
