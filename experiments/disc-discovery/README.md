# Disc discovery experiment

This standalone, unintegrated adapter looks for a US Disc 1 by reading its ISO9660 volume descriptor through PSXRecomp's existing disc reader. It scans a bounded directory, handles arbitrary filenames, treats a CUE and its data file as one image, and reports ambiguous matches. It does not change the current EBOOT, startup paths or saves.

Local host fixtures passed for synthetic raw BIN, cooked ISO and CUE images, invalid and missing inputs, and duplicate detection. Both adapter translation units compiled for PS5; they have not been linked into the game or tested on a console. CHD was stubbed out in the host tests. A recognized descriptor does not guarantee that an image contains all data needed for gameplay or audio. Integration with the actual portable installation and root selection remains outstanding.

The Python fixtures expect `probe.exe` and `discovery.exe` built from the included test entry points, adapters and the pinned PSXRecomp runtime reader, CUE and path sources. `chd_test_stubs.cpp` is a test-only substitute and does not implement CHD support. Run `test_probe.py` first to generate synthetic fixtures, then `test_discovery.py`. No game data is included.
