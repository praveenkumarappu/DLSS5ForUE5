#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDLSS5, Log, All);

class FDLSS5NRViewExtension;

class FDLSS5ForUE5Module final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void HandlePostEngineInit();
    bool TickDiagnostics(float DeltaTime);

    TSharedPtr<FDLSS5NRViewExtension, ESPMode::ThreadSafe> ViewExtension;
    FDelegateHandle PostEngineInitHandle;
    FTSTicker::FDelegateHandle DiagnosticsTickerHandle;
};
