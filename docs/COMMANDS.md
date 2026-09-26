# Console Commands and CVars

## Commands

```text
DLSS5.Open      Open the editor control window
DLSS5.Status    Print runtime / guide diagnostics
DLSS5.SafeMode  Disable NR and experimental guides
```

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
r.DLSS5.HighlightPreserveStart
r.DLSS5.HighlightPreserveEnd
r.DLSS5.ControlMaskStencil
r.DLSS5.ControlMaskInvert
r.DLSS5.DistortionFieldTexture
r.DLSS5.ScalingRatio
r.DLSS5.ScalingRatioMode
r.DLSS5.Debug
```

The `r.DLSS5.*` values in `DefaultEngine.ini` (`[/Script/Engine.RendererSettings]`) are the launch
state. The editor window mirrors them when opened and pushes edits back to the CVars; it does not
save to config. Safe boot (everything forced off) only engages when the previous session left
`Saved/DLSS5NR/session.lock` behind, i.e. crashed.

## Guides and masks

```text
r.DLSS5.ControlMaskStencil       0 off, -1 any non-zero custom stencil, 1..255 that stencil value
r.DLSS5.ControlMaskInvert        1.0 outside the tagged actors instead of inside
r.DLSS5.DistortionFieldTexture   asset path of an RGBA16F texture: RG distorted->undistorted, BA inverse, display pixels
r.DLSS5.HighlightPreserveStart   display brightness where the tonemapped colour starts blending back (0.7)
r.DLSS5.HighlightPreserveEnd     display brightness where it fully replaces NR (0.9); Start >= End disables
r.DLSS5.ScalingRatio             experimental; mode 0 is rejected by the current runtime, mode 1 has no measured effect
```

Motion vectors are fed in display-pixel units with `r.DLSS5.MVecScaleX/Y` at 1.0 (the NGX
convention). Depth and motion guides are resolved at the colour resolution.

## Pass Count

```text
r.DLSS5.PassCount 1
r.DLSS5.PassCount 2
r.DLSS5.PassCount 3
r.DLSS5.PassCount 4
```
