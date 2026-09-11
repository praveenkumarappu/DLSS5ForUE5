#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

class IDetailsView;
class FReply;
class SDockTab;
class FSpawnTabArgs;

class FDLSS5ForUE5EditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OpenWindow();
    TSharedRef<SDockTab> SpawnPluginTab(const FSpawnTabArgs& Args);
    bool TickWindow(float DeltaTime);
    FReply EnterSafeMode();

    TSharedPtr<IDetailsView> DetailsView;
    FTSTicker::FDelegateHandle WindowTickerHandle;
};
