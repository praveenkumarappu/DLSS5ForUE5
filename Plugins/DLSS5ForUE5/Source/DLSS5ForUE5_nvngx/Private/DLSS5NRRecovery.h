#pragma once
#include "CoreMinimal.h"

namespace DLSS5NRRecovery
{
    inline bool IsRecoverable(bool bValid, bool bProcessRunning, double AgeDays)
    { return bValid && !bProcessRunning && AgeDays >= 0.0 && AgeDays <= 7.0; }
    bool Begin(FString& OwnedMarker);
    void Update(const FString& OwnedMarker);
    void End(const FString& OwnedMarker);
}
