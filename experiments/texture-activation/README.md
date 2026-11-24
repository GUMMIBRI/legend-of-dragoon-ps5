# Experimental Vulkan texture activation

This patch enables the existing texture-replacement provider when Vulkan has initialized and the app's configured texture root contains a `replacements` directory. Missing packs leave original rendering in place. It applies to the retained portable PS5 source snapshot, not directly to the pinned upstream tree without reconciliation.

The resulting PS5 translation unit compiled and an ELF linked against the retained portable build objects. An isolated activation fixture passed for absent roots, absent folders, a present folder and inactive Vulkan. The fixture stubs the provider; it does not establish image decoding, scene matching, foreground behavior, performance, or console operation. The linked ELF is not signed, packaged or installed. App-root discovery remains unverified.

No texture pack is included. The examined user-provided SCUS-94491 archive uses DuckStation-style filenames, but extraction reported checksum failures; its usable coverage and live matching remain unverified. This change is not direct Severed Chains pack support.

## Pack inspection and limitations

The archive listed 8,412 PNG files; 8,355 were present after extraction reported checksum failures. The difference is a file-presence count, not a claim that each absent file independently failed its checksum. The actual DuckStation scanner recognized 8,334 entries, reported one ambiguous entry and ignored 11. A separate scan decoded all 8,355 extracted PNGs successfully with the runtime PNG decoder. These checks do not show that any replacement matches a game draw or that foreground layers render correctly.

`scan.cpp` runs the real DuckStation scanner with image decoding stubbed. `decode-scan.cpp` runs the real runtime decoder and STB for PNGs, with WebP calls stubbed. These are inspection harnesses; neither tests GPU rendering. The checks used the local modified PSXRecomp framework, not a clean checkout of the pinned upstream revision; reproducing them requires the matching framework modifications. Run them against a user-supplied extracted pack. They do not include images, game data, executables or proprietary SDK components.

For example, with a C++17 compiler and the matching modified framework in `framework`:

```sh
c++ -std=c++17 -Iframework/runtime/include experiments/texture-activation/scan.cpp framework/runtime/src/duckstation_texture_pack.cpp -o scan
c++ -std=c++17 -Iframework/runtime/include -I/path/to/libwebp/src experiments/texture-activation/decode-scan.cpp framework/runtime/src/texture_image_decode.cpp framework/runtime/tests/texture_install_stb.cpp -o decode-scan
./scan /path/to/pack-root
./decode-scan /path/to/pack-root/replacements
```

These commands describe the source inputs; compiler and thread-linker flags can vary by platform. No dependency download is performed by the harnesses.

The linked review ELF also passed the static PS5 runtime audit with no inline system calls. That is not a console test or confirmation that application-folder discovery works. A complete reproducible build recipe and installable candidate remain outstanding.

`identical-duplicates.patch` is a separate experimental change against the retained portable framework source. The examined pack contains one duplicated texture key whose two PNGs are byte-identical. The patch compares bounded file contents and retains a single entry when they are identical; differing or unreadable copies remain ambiguous. Host fixtures passed identical and conflicting duplicate cases, the actual pack scan reported zero ambiguous entries, and the changed PS5 translation unit compiled and linked. This newer ELF has not yet undergone its own runtime audit or console test. The 11 rejected files require separate texture-page compatibility work.
