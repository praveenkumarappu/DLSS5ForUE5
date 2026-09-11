#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FRDGTexture;
class FSceneView;
using FRDGTextureRef = FRDGTexture*;

/** Copies UE device depth for the active render rect into a compact R32_FLOAT guide texture. */
FRDGTextureRef AddDLSS5NRDepthResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    const FIntRect& ViewRect);

/**
 * Builds a dense, pixel-space motion-vector texture from UE's sparse/encoded velocity GBuffer.
 * Static/background pixels are reconstructed from depth + View.ClipToPrevClip, following the
 * dense motion-vector resolve recommended in NVIDIA's DLSS integration guidance for Unreal.
 * If UE provides only its 1x1 dummy velocity texture, camera motion is still reconstructed.
 */
FRDGTextureRef AddDLSS5NRMotionVectorResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    FRDGTextureRef SceneVelocity,
    const FIntRect& ViewRect);
