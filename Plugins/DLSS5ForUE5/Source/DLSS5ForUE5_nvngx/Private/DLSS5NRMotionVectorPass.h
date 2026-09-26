#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FRDGTexture;
class FSceneView;
using FRDGTextureRef = FRDGTexture*;

/**
 * Resamples UE device depth for the active render rect into a zero-based R32_FLOAT guide texture
 * at OutputSize (the NR colour resolution).
 */
FRDGTextureRef AddDLSS5NRDepthResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    const FIntRect& ViewRect,
    const FIntPoint& OutputSize);

/**
 * Builds a dense motion-vector texture at OutputSize (the NR colour resolution), in display-pixel
 * units, from UE's sparse/encoded velocity GBuffer. Static/background pixels are reconstructed from
 * depth + View.ClipToPrevClip, matching NVIDIA's UE DLSS plugin VelocityCombine pass.
 * If UE provides only its 1x1 dummy velocity texture, camera motion is still reconstructed.
 */
FRDGTextureRef AddDLSS5NRMotionVectorResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    FRDGTextureRef SceneVelocity,
    const FIntRect& ViewRect,
    const FIntPoint& OutputSize);

/**
 * Resolves UE custom depth stencil into an R8 control-mask guide at OutputSize. StencilValue < 0
 * matches any non-zero stencil; otherwise the exact value. Returns nullptr if the stencil SRV is
 * missing or the engine's 1x1 dummy.
 */
FRDGTextureRef AddDLSS5NRControlMaskResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureSRVRef CustomStencil,
    const FIntRect& ViewRect,
    const FIntPoint& OutputSize,
    int32 StencilValue,
    bool bInvert);

/**
 * Blends the original colour back over the NR result for bright pixels (display-encoded thresholds)
 * and restores the tonemapper alpha. Returns a new texture with the same desc as NRColor.
 */
FRDGTextureRef AddDLSS5NRHighlightPreservePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef InputColor,
    FRDGTextureRef NRColor,
    const FIntRect& Rect,
    float PreserveStart,
    float PreserveEnd);
