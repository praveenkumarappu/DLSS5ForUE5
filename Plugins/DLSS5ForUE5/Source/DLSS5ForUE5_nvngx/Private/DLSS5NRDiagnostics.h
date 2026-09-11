#pragma once

#include "CoreMinimal.h"

struct FDLSS5NRFrameTelemetry
{
    bool bDepthAvailable = false;
    bool bMotionAvailable = false;
    bool bDepthRequested = false;
    bool bMotionRequested = false;
    bool bReset = false;
    bool bCameraCut = false;
    bool bForceCameraReset = false;
    FIntRect ColorRect;
    FIntRect DepthRect;
    FIntRect MotionRect;
    FIntRect OutputRect;
    FIntPoint SceneDepthExtent = FIntPoint::ZeroValue;
    FIntPoint SceneVelocityExtent = FIntPoint::ZeroValue;
    FVector2f JitterPixels = FVector2f::ZeroVector;
    float PreExposure = 1.0f;
    float MotionScaleX = 1.0f;
    float MotionScaleY = 1.0f;
    FString ResetReason;
};

struct FDLSS5NRDiagnosticsSnapshot
{
    bool bRuntimeLoaded = false;
    bool bRuntimeInitialized = false;
    bool bCoreFound = false;
    bool bCoreParameters = false;
    bool bSnippetPopulated = false;
    bool bCallerGateReady = false;

    uint64 EvaluateAttempts = 0;
    uint64 SuccessfulFrames = 0;
    uint64 FailedFrames = 0;
    uint64 ResetFrames = 0;

    bool bDepthAvailable = false;
    bool bMotionAvailable = false;
    bool bDepthRequested = false;
    bool bMotionRequested = false;
    bool bDepthUsedLastFrame = false;
    bool bMotionUsedLastFrame = false;
    bool bResetLastFrame = false;
    bool bCameraCutLastFrame = false;

    FIntPoint FeatureSize = FIntPoint::ZeroValue;
    FIntPoint SceneDepthExtent = FIntPoint::ZeroValue;
    FIntPoint SceneVelocityExtent = FIntPoint::ZeroValue;
    FIntRect ColorRect;
    FIntRect DepthRect;
    FIntRect MotionRect;
    FIntRect OutputRect;
    FVector2f JitterPixels = FVector2f::ZeroVector;
    float PreExposure = 1.0f;
    float MotionScaleX = 1.0f;
    float MotionScaleY = 1.0f;

    uint32 LastNGXResult = 0;
    FString LastError;
    FString RuntimePath;
    FString CallerModulePath;
    FString LastResetReason;
};
