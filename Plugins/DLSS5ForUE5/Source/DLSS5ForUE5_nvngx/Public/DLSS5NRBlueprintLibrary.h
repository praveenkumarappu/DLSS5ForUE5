#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DLSS5NRBlueprintLibrary.generated.h"

UCLASS()
class DLSS5FORUE5_NVNGX_API UDLSS5NRBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Changes requested state; success does not mean NGX inference is available.
    UFUNCTION(BlueprintCallable, Category="DLSS5")
    static bool SetNeuralRenderingEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="DLSS5")
    static FString GetNeuralRenderingStatus();
};
