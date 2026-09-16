// Print memory blocks, image base, entry points and function count for the
// current program (works with the lx-loader for LE/LX images).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class DumpInfo extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("program : " + currentProgram.getName());
        println("language: " + currentProgram.getLanguageID());
        println("compiler: " + currentProgram.getCompilerSpec().getCompilerSpecID());
        println("imageBase: " + currentProgram.getImageBase());
        println("minAddr : " + currentProgram.getMinAddress());
        println("maxAddr : " + currentProgram.getMaxAddress());
        println("-- memory blocks --");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println(String.format("  %-24s %s - %s  size=%d  r=%b w=%b x=%b init=%b",
                    b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                    b.isRead(), b.isWrite(), b.isExecute(), b.isInitialized()));
        }
        println("-- entry points --");
        for (Address a : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
            println("  " + a);
        }
        println("-- function count --");
        println("  " + currentProgram.getFunctionManager().getFunctionCount());
    }
}
