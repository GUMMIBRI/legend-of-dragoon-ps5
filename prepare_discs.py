"""Build a local Dragoon disc payload for /mnt/usb0/lod/disc."""

import argparse
import shutil
import tempfile
import zipfile
from pathlib import Path


DISC_IDENTITIES = (
    (484951824, 0x24CB8F5C),
    (576009504, 0x8139792A),
    (559173888, 0x405DA729),
    (748204128, 0x7430EAED),
)


def prepare(source, destination):
    if destination.exists():
        raise FileExistsError(destination)
    with tempfile.TemporaryDirectory(prefix="dragoon-", dir=destination.parent) as temp:
        payload = Path(temp) / "disc"
        payload.mkdir()
        for number, identity in enumerate(DISC_IDENTITIES, start=1):
            original = f"Legend of Dragoon, The (USA) (Disc {number})"
            with zipfile.ZipFile(source / f"{original}.zip") as archive:
                image = archive.getinfo(f"{original}.bin")
                if (image.file_size, image.CRC) != identity:
                    raise ValueError(f"Unexpected image identity for disc {number}")
                cue = archive.read(f"{original}.cue").decode("ascii")
                expected_cue = (
                    f'FILE "{original}.bin" BINARY\n'
                    "  TRACK 01 MODE2/2352\n"
                    "    INDEX 01 00:00:00"
                )
                if cue.replace("\r\n", "\n").strip() != expected_cue:
                    raise ValueError(f"Unexpected CUE layout for disc {number}")
                # Reading to EOF makes ZipFile verify the archive member's CRC.
                with archive.open(image) as stream, (payload / f"disc{number}.bin").open("xb") as output:
                    shutil.copyfileobj(stream, output, length=1024 * 1024)
                (payload / f"disc{number}.cue").write_text(
                    expected_cue.replace(f"{original}.bin", f"disc{number}.bin") + "\n",
                    encoding="ascii",
                )
        payload.rename(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Read-only directory containing the four US ZIPs")
    args = parser.parse_args()
    stage = Path(__file__).resolve().parent / "local-usb-payload" / "lod"
    stage.mkdir(parents=True, exist_ok=True)
    prepare(args.source, stage / "disc")
