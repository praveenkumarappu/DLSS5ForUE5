# Console Commands and CVars

## Commands

```text
DLSS5.Open      Editor only: open the editor control window
DLSS5.Status    Print runtime / guide diagnostics
DLSS5.SafeMode  Disable NR and experimental guides
```

`DLSS5.Open` is registered by the Editor module and is unavailable in packaged or `-game` runtime targets. `DLSS5.Status` is the runtime diagnostic command; its availability and logging in Shipping builds depend on the host's console and build settings.

## Main CVars

```text
r.DLSS5.Enable
r.DLSS5.Preset
r.DLSS5.PassCount
r.DLSS5.Intensity
r.DLSS5.LocalTone
r.DLSS5.LocalStructure
r.DLSS5.SkinStructure
r.DLSS5.AutoMask
r.DLSS5.Style
r.DLSS5.UICorrection
r.DLSS5.EnableSceneGuides
r.DLSS5.FeedSceneGuides
r.DLSS5.UseDepth
r.DLSS5.UseMotionVectors
r.DLSS5.AutoResetHistory
r.DLSS5.DepthInverted
r.DLSS5.MVecScaleX
r.DLSS5.MVecScaleY
r.DLSS5.ExposureResetRatio
r.DLSS5.Debug
r.DLSS5.HighlightPreserve
r.DLSS5.HighlightPreserveStart
r.DLSS5.HighlightPreserveEnd
```

## Pass Count

```text
r.DLSS5.PassCount 1
r.DLSS5.PassCount 2
r.DLSS5.PassCount 3
r.DLSS5.PassCount 4
```

## Optional highlight preservation

`r.DLSS5.HighlightPreserve` toggles restoration of bright highlights over the Neural Rendering output (`0` off, `1` on). The `HighlightPreserveStart` and `HighlightPreserveEnd` controls set the blend thresholds in the **current post-tonemap color-buffer units**, not absolute scene luminance or HDR nits. The end threshold must exceed the start threshold for recovery to apply.
