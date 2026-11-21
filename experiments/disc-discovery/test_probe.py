from pathlib import Path
import subprocess
p=Path(__file__).resolve().parent;d=p/"fixtures";d.mkdir(exist_ok=True)
def pvd(volume):
 b=bytearray(2048);b[0]=1;b[1:6]=b"CD001";b[6]=1;b[40:49]=volume;return b
def raw(name,volume):
 data=bytearray(20*2352);
 for i in range(20):
  k=i*2352;data[k:k+12]=b"\x00"+b"\xff"*10+b"\x00";data[k+15]=2
 data[16*2352+24:16*2352+24+2048]=pvd(volume);(d/name).write_bytes(data)
def cooked(name,volume):
 data=bytearray(20*2048);data[16*2048:17*2048]=pvd(volume);(d/name).write_bytes(data)
raw("arbitrary name.bin",b"SCUS94491");raw("wrong.bin",b"SCUS94584");raw("wrong-tail.bin",b"SCUS94491");tail=bytearray((d/"wrong-tail.bin").read_bytes());tail[16*2352+24+49]=ord("X");(d/"wrong-tail.bin").write_bytes(tail);cooked("Disc One.ISO",b"SCUS94491");(d/"sample.cue").write_text('FILE "arbitrary name.bin" BINARY\n TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n');(d/"truncated.iso").write_bytes(b"short");(d/"bad.cue").write_text('FILE "missing.bin" BINARY\n TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n')
for name,expected in [("arbitrary name.bin",0),("Disc One.ISO",0),("sample.cue",0),("wrong.bin",1),("wrong-tail.bin",1),("truncated.iso",2),("bad.cue",2),("missing.bin",2)]:
 r=subprocess.run([str(p/"probe.exe"),str(d/name)],capture_output=True,text=True);print(name,r.returncode,r.stdout.strip());assert r.returncode==expected,(name,r.stdout,r.stderr)
print("PASS actual ISOReader raw BIN, cooked ISO, CUE, wrong disc, truncated and missing inputs; CHD mocked unavailable")
