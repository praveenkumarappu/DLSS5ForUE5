#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDLSS5, Log, All);

DLSS5FORUE5_NVNGX_API FString DLSS5GetVersion();

class FDLSS5NRViewExtension;

class FDLSS5ForUE5Module final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    FString RecoveryMarker;
    void HandlePostEngineInit();
    bool TickDiagnostics(float DeltaTime);

    TSharedPtr<FDLSS5NRViewExtension, ESPMode::ThreadSafe> ViewExtension;
    FDelegateHandle PostEngineInitHandle;
    FTSTicker::FDelegateHandle DiagnosticsTickerHandle;
};
