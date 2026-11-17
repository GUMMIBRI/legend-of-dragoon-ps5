# Legend of Dragoon PS5 — development source

Source snapshot of the native PS5 port and its Severed Chains disc-directory compatibility change. This is not a game download, a store release or a claim that all features work.

## Architecture and source

The current native PS5 build uses PSXRecomp. Severed Chains is a separate Java engine used as an artwork/format reference; its small unpacker patch is included separately. These are distinct upstream projects, with their original licenses and credits retained. This repository does not claim that the native port is the complete Severed Chains engine or supports all its mods.

`UPSTREAM.json` pins both upstream source revisions. Clone the named repositories into `framework` and `severed-chains`, check out the recorded commits, then apply the corresponding files in `patches/` with `git apply`. The root source implements PS5 storage/startup, menus and field/background integration. The framework patch includes the Vulkan/movie presentation, frame-owned staging, scoped duplicate-menu suppression, audio diagnostics and related source changes. Uninstalled work is included as development source, not as tested console behavior.

## Building and limitations

Use your own supported US Disc 1. Original disc images, extracted executable/data, translated game output, saves, settings, logs, private artwork, proprietary modules and built EBOOTs are excluded. The source configuration and generator references show where privately generated inputs belong. Use OpenBIOS under its own MIT notice; no original console BIOS is supplied here.

`build-ps5-title.ps1` records the existing developer build recipe. It currently expects a separately provisioned PS5 homebrew SDK/LLVM/RADV/native finalizer and an existing CMake/Ninja build tree. Its local prefix paths are not a portable installer. Reconstructing a fresh build environment and complete own-disc preparation remain work; this snapshot does not claim a one-command clean-machine build.

Console observations include improved full-screen movies, with remaining video/audio skips, cutscene-skip limitations and menu issues. Scoped duplicate-menu and staging changes are local; the successor still has an unresolved native runtime audit involving filesystem calls. HD scene binding and foreground composition remain unqualified. Do not infer a fixed game or a released binary from this source publication.

## Attribution and licenses

PSXRecomp is by RetroPortingToolKit and contributors, under its PolyForm Noncommercial license and additional permissions. Framework modifications retain those notices. The Severed Chains patch is based on Legend-of-Dragoon-Modding/Severed-Chains and retains AGPL-3.0 coverage. Bundled AMD FSR source keeps its own upstream license. See `licenses/` and the notices in the pinned upstream trees. Third-party permissions do not grant rights to the original game.
