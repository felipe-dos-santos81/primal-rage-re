// Print hex bytes at given addresses. args: addrHex,length [addrHex,length ...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpBytes extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        for (int i = 0; i + 1 < a.length; i += 2) {
            Address ad = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(a[i]);
            int len = (int) Long.parseLong(a[i + 1], 16);
            byte[] b = new byte[len];
            currentProgram.getMemory().getBytes(ad, b);
            StringBuilder sb = new StringBuilder();
            for (byte x : b) sb.append(String.format("%02x", x));
            println(a[i] + " " + sb);
        }
    }
}
