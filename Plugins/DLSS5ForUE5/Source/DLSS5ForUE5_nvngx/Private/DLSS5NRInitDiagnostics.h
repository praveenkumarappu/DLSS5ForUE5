#pragma once

#include "CoreMinimal.h"
#include "DLSS5NRNGXABI.h"

struct ID3D12Device;

// Read-only, opt-in evidence. Does not alter NGX parameters, device flags or authorization.
void DLSS5NRWriteInitDiagnostic(const TCHAR* Stage, ID3D12Device* Device,
    const FNGXParameter* Parameters, const FString& RuntimePath,
    const FString& CallerPath, uint64 AppId, uint32 ApiVersion, FNGXResult Result);
