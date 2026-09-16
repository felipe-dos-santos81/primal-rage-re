// Print functions and instructions in an address range.
// args: <startHex> <endHex>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class InspectAddr extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(a[0]);
        Address end = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(a[1]);
        println("-- functions overlapping range --");
        Function f = getFunctionContaining(start);
        if (f != null && f.getEntryPoint().getOffset() < start.getOffset()) f = getFunctionAfter(f);
        while (f != null && f.getEntryPoint().compareTo(end) <= 0) {
            println("  " + f.getName() + "  " + f.getEntryPoint() + "-" + f.getBody().getMaxAddress()
                    + " size=" + f.getBody().getNumAddresses());
            f = getFunctionAfter(f);
        }
        println("-- instructions --");
        Instruction ins = getInstructionAt(start);
        if (ins == null) ins = getInstructionAfter(start);
        int n = 0;
        while (ins != null && ins.getAddress().compareTo(end) <= 0 && n < 80) {
            println("  " + ins.toString());
            ins = ins.getNext();
            n++;
        }
        if (ins == null) println("  (no instruction)");
    }
}
