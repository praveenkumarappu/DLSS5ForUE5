# Third-Party Notices

DLSS5ForUE5 is an unofficial community project and is not affiliated with, sponsored by, or endorsed by NVIDIA Corporation or Epic Games, Inc.

## NVIDIA

NVIDIA, GeForce, RTX, DLSS, and NGX are trademarks and/or technologies of NVIDIA Corporation and its affiliates.

Precompiled DLSS5ForUE5 release packages may include NVIDIA runtime components required for the plugin to operate. Those NVIDIA binaries are provided only as runtime components of the plugin and are **not** licensed under the DLSS5ForUE5 MIT License.

NVIDIA software remains subject to NVIDIA's applicable license terms. The original DLSS5ForUE5 source code, editor integration, UE render passes, controls, diagnostics, and compatibility code are separate from NVIDIA's proprietary runtime.

NVIDIA RTX SDK license reference:
https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt

The NVIDIA DLSS Unreal Engine plugin itself is not part of this repository and must be installed separately by the user.

### v0.6.1 clarification

The general notice above is preserved from the published v0.6.0 release for continuity. It is not itself a license grant for any NVIDIA runtime. The exact v0.6.1 DLL has additional provenance and redistribution questions documented below. See the specific release gate in that section before distributing the DLL-containing packages.

## NVIDIA Neural Rendering runtime: v0.6.1 artifact provenance

The v0.6.1 Editor Win64 archives were independently checked for archive integrity and contain the following experimental DLL:

| Property | Observed value |
| --- | --- |
| Filename | `nvngx_dlssnr.dll` |
| Reported PE file version | `310.9.16.0` |
| File size | `276,162,560` bytes |
| SHA-256 | `cdfa6c1ec784867182164c7836f324f087b5c9f374d0c9fe46f54999feabf3ed` |
| Authenticode | `NotSigned` (observed in provided verification) |
| Status | Community compatibility build; not demonstrated to be the unmodified NVIDIA SDK binary |

### Source identity and independent project documentation

The exact SHA-256 above is the **reference output** for the `compat-fp16-310.9.16` transformation documented by [ShortFuse / OpenNR](https://github.com/clshortfuse/openNR). That project reports an original input of **266,424,944 bytes** with SHA-256 `2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f`; the output is **not byte-identical** to that input. OpenNR states it does **not distribute NVIDIA DLLs or generated compatibility DLLs**. Matching a published reference-output hash establishes technical identity with those output bytes, not how our particular copy was obtained or permission to distribute it.

Other public projects document distinct choices:

- [Veyra-NRVideo, THIRD_PARTY_NOTICES.md](https://github.com/Likely7/Veyra-NRVideo/blob/main/THIRD_PARTY_NOTICES.md) identifies this **same hash, size and version** as a user-supplied ShortFuse community-modified runtime used in local testing. Its notice expressly distinguishes local authorization from public redistribution permission.
- [VRChat-DLSS5-Cam, THIRD_PARTY_NOTICES.md](https://github.com/AlanBacker/VRChat-DLSS5-Cam/blob/main/THIRD_PARTY_NOTICES.md) describes bundled NVIDIA NR 310.8.0.0 binaries (including a community-adapted build), preserves NVIDIA attribution and treats the runtime separately from its MIT-licensed code. Those binaries are **not the same v0.6.1 hash**.
- [Merserk / dlss5-visual-enhancer releases](https://github.com/Merserk/dlss5-visual-enhancer/releases) publicly list an NR 310.9.16.0 update. Its [README](https://github.com/Merserk/dlss5-visual-enhancer) refers to third-party notices. The release version alone does **not establish that its DLL matches this hash or that NVIDIA has granted transferable permission**.
- [bodycam-dlss-framegen-mod, THIRD_PARTY_NOTICES.md](https://github.com/AriesAlex/bodycam-dlss-framegen-mod/blob/main/THIRD_PARTY_NOTICES.md) distinguishes redistributable components obtained from the official NVIDIA Streamline SDK from the experimental NR DLL, **which it excludes from public packages**.

These external repositories are provided as **attribution/provenance comparisons only**. Their presence on GitHub, their licenses for project-owned code, and their release practices are **not authorization** for this project to distribute third-party DLLs.

### Applicable NVIDIA terms and release gate

The [NVIDIA RTX SDKs License](https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt) provides a conditional grant for distributing **software and materials actually supplied under the applicable SDK license**, as incorporated into applications, subject to its distribution requirements and restrictions. NVIDIA's [official DLSS SDK releases](https://github.com/NVIDIA/DLSS/releases/tag/v310.9.16) do not by themselves establish that this specific **community-modified `nvngx_dlssnr.dll`** is an authorized redistributable SDK component.

Because our matching v0.6.1 bytes correspond to a community compatibility output, we have **not established authorization from NVIDIA or another relevant rights holder to redistribute this modified binary**. Attribution, SHA-256 verification, a proprietary notice, prior use in games, and third-party GitHub release examples do not resolve that question.

**Public release rule:** Do not publish the existing four DLL-containing v0.6.1 ZIPs unless the license/provenance review supplies evidence that permits distribution of **these exact bytes** with this plugin. Alternatively, prepare clearly labelled DLL-free release ZIPs from the verified source and compiled modules, if plugin loading and runtime-not-found handling are tested, and document lawful user-supplied runtime configuration. Do not silently change the validated Windows binaries or misrepresent a DLL-free package as runtime-tested.

This is a distribution-rights gate, **not a claim that the maintainer's UE 5.6/5.8 viewport or MRQ tests failed**.

## Epic Games / Unreal Engine

Unreal Engine is a trademark or registered trademark of Epic Games, Inc. Unreal Engine source, headers, tools, and binaries remain governed by Epic's applicable Unreal Engine license terms.

## No endorsement

Use of third-party names describes interoperability only. DLSS5ForUE5 must not be represented as an official NVIDIA or Epic Games plugin.
