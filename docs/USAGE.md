# Usage

## Recommended sequence

Start with one pass and introduce experimental inputs gradually:

```text
1. Enable Neural Rendering
2. Enable 3D Scene Guides
3. Keep Feed Scene Guides OFF and verify stability
4. Feed Scene Guides ON with Depth ON / Motion OFF
5. Enable Motion Vectors
6. Increase Pass Count only after 1-pass is stable
```

## Pass Count

Pass Count can be set from 1 to 4. Passes are sequential:

```text
UE color → NR Pass 1 → NR Pass 2 → NR Pass 3 → NR Pass 4 → output
```

Each additional pass consumes the previous pass output. GPU cost rises significantly and the visual result can become stronger or more temporal. More passes are not automatically higher quality.

## Scene Guides

- **Enable 3D Scene Guides:** master experimental guide switch
- **Feed Scene Guides Into NR:** allows generated guides to reach NGX
- **Use DLSS Input Depth:** UE SceneDepth, which is also the host depth source used by DLSS
- **Use DLSS-Compatible Motion Vectors:** dense guide built from UE depth, camera reprojection and available velocity data
- **Automatic History Reset:** resets temporal state on camera cuts, viewport changes, first-frame transitions and major exposure changes

## Live Diagnostics

Healthy operation generally shows:

```text
Runtime READY | Core READY | Core params READY | Caller gate READY
Initialized YES | Snippet YES | Feature <resolution>
... successful increasing | 0 failed
Last Result 0x00000001
```

## Safe Mode

If a previous experimental state becomes unstable, press **Safe Mode** or execute:

```text
DLSS5.SafeMode
```

This disables Neural Rendering and experimental guide execution.
