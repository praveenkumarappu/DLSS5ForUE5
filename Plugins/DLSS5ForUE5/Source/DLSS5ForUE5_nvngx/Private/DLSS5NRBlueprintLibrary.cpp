#include "DLSS5NRBlueprintLibrary.h"
#include "DLSS5NRRuntime.h"
#include "HAL/IConsoleManager.h"

bool UDLSS5NRBlueprintLibrary::SetNeuralRenderingEnabled(bool bEnabled)
{
    if(!IsInGameThread())return false;
    IConsoleVariable* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.DLSS5.Enable"));
    if(!V)return false;
    V->Set(bEnabled ? 1 : 0,ECVF_SetByConsole);
    return V->GetInt()==(bEnabled ? 1 : 0);
}
FString UDLSS5NRBlueprintLibrary::GetNeuralRenderingStatus()
{
    const auto D=FDLSS5NRRuntime::Get().GetDiagnosticsSnapshot();
    return FString::Printf(TEXT("RuntimeLoaded=%d Initialized=%d SuccessfulEvaluations=%llu LastNGX=0x%08X Error=%s"),
        D.bRuntimeLoaded,D.bRuntimeInitialized,D.SuccessfulFrames,D.LastNGXResult,*D.LastError);
}
