# Experimental Vulkan texture activation

This patch enables the existing texture-replacement provider when Vulkan has initialized and the app's configured texture root contains a `replacements` directory. Missing packs leave original rendering in place. It applies to the retained portable PS5 source snapshot, not directly to the pinned upstream tree without reconciliation.

The resulting PS5 translation unit compiled and an ELF linked against the retained portable build objects. An isolated activation fixture passed for absent roots, absent folders, a present folder and inactive Vulkan. The fixture stubs the provider; it does not establish image decoding, scene matching, foreground behavior, performance, or console operation. The linked ELF is not signed, packaged or installed. App-root discovery remains unverified.

No texture pack is included. The examined user-provided SCUS-94491 archive uses DuckStation-style filenames, but extraction reported checksum failures; its usable coverage and live matching remain unverified. This change is not direct Severed Chains pack support.
