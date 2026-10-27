#include "Misc/AutomationTest.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS
class FDLSS5NRValidationCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FDLSS5NRValidationCS);
    SHADER_USE_PARAMETER_STRUCT(FDLSS5NRValidationCS,FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, Results)
    END_SHADER_PARAMETER_STRUCT()
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
    {return IsFeatureLevelSupported(P.Platform,ERHIFeatureLevel::SM5);}
};
IMPLEMENT_GLOBAL_SHADER(FDLSS5NRValidationCS,"/DLSS5NR/Private/DLSS5NRValidation.usf","ValidationMainCS",SF_Compute);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDLSS5NRGuideGPUTest,"DLSS5.GPU.MotionGuideContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FDLSS5NRGuideGPUTest::RunTest(const FString&)
{
    TArray<FVector4f> Results;
    bool bRead = false;
    ENQUEUE_RENDER_COMMAND(DLSS5NRValidateGuides)([&Results,&bRead](FRHICommandListImmediate& RHICmdList)
    {
        FRHIGPUBufferReadback Readback(TEXT("DLSS5NR.Validation"));
        FRDGBuilder Graph(RHICmdList);
        auto Buffer=Graph.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f),16),TEXT("DLSS5NR.TestResults"));
        auto* P=Graph.AllocParameters<FDLSS5NRValidationCS::FParameters>();P->Results=Graph.CreateUAV(Buffer);
        TShaderMapRef<FDLSS5NRValidationCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FComputeShaderUtils::AddPass(Graph,RDG_EVENT_NAME("DLSS5NR GPU Guide Contract"),Shader,P,FIntVector(1,1,1));
        AddEnqueueCopyPass(Graph,&Readback,Buffer,16*sizeof(FVector4f));
        Graph.Execute();RHICmdList.SubmitAndBlockUntilGPUIdle();
        if(Readback.IsReady())
        {
            const void* Data=Readback.Lock(16*sizeof(FVector4f));
            if(Data){Results.SetNum(16);FMemory::Memcpy(Results.GetData(),Data,16*sizeof(FVector4f));bRead=true;}
            Readback.Unlock();
        }
    });
    FlushRenderingCommands();
    if(!TestTrue(TEXT("Read real GPU results"),bRead))return false;
    for(int32 i=0;i<16;++i)
    {
        const float ExpectedX=i<8 ? -float(i-3) : 9.6f;
        const float ExpectedY=i<8 ? -2.0f*float(i-3) : 10.8f;
        TestTrue(*FString::Printf(TEXT("pixel X case %d"),i),FMath::IsNearlyEqual(Results[i].X,ExpectedX,0.003f));
        TestTrue(*FString::Printf(TEXT("pixel Y case %d"),i),FMath::IsNearlyEqual(Results[i].Y,ExpectedY,0.003f));
        TestEqual(TEXT("Guide mapping X with subrect offset"),int32(Results[i].Z),32+FMath::Min((((i%8)*400+1)*1280)/(1920*2),1279));
        TestEqual(TEXT("Guide mapping Y with subrect offset"),int32(Results[i].W),16+FMath::Min((((i%8)*200+1)*720)/(1080*2),719));
    }
    return true;
}
#endif
