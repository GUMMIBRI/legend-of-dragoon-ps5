from pathlib import Path
import shutil,subprocess
p=Path(__file__).resolve().parent;src=p/"fixtures";base=p/"discovery-fixtures";base.mkdir(exist_ok=True)
def scenario(name,files,expected,contains=""):
 d=base/name;d.mkdir(exist_ok=True)
 for name in files:shutil.copyfile(src/name,d/name)
 r=subprocess.run([str(p/"discovery.exe"),str(d)],capture_output=True,text=True);print(d.name,r.returncode,r.stdout.strip());assert r.returncode==expected and contains in r.stdout,(r.stdout,r.stderr)
scenario("empty",[],1)
scenario("arbitrary-bin",["arbitrary name.bin"],0,"arbitrary name.bin")
scenario("arbitrary-iso",["Disc One.ISO"],0,"Disc One.ISO")
scenario("paired",["arbitrary name.bin","sample.cue"],0,"sample.cue")
scenario("wrong",["wrong.bin"],1)
scenario("duplicate",["arbitrary name.bin","Disc One.ISO"],2,"Multiple valid")
scenario("bad-cue",["bad.cue"],3,"Cannot inspect")
print("PASS actual reader folder selection, CUE+BIN deduplication and ambiguity")
