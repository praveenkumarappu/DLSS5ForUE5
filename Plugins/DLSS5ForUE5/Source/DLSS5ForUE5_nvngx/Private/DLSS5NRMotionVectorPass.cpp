#include "DLSS5NRMotionVectorPass.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

class FDLSS5NRDepthCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FDLSS5NRDepthCS);
    SHADER_USE_PARAMETER_STRUCT(FDLSS5NRDepthCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
        SHADER_PARAMETER(FIntPoint, SourceRectMin)
        SHADER_PARAMETER(FIntPoint, SourceRectSize)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputDepth)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FDLSS5NRMotionVectorCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FDLSS5NRMotionVectorCS);
    SHADER_USE_PARAMETER_STRUCT(FDLSS5NRMotionVectorCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VelocityTexture)
        SHADER_PARAMETER(FIntPoint, SourceRectMin)
        SHADER_PARAMETER(FIntPoint, SourceRectSize)
        SHADER_PARAMETER(uint32, HasObjectVelocity)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, OutputMotionVectors)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FDLSS5NRDepthCS, "/DLSS5NR/Private/DLSS5NRDepth.usf", "DepthMainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDLSS5NRMotionVectorCS, "/DLSS5NR/Private/DLSS5NRMotionVectors.usf", "MotionMainCS", SF_Compute);

FRDGTextureRef AddDLSS5NRDepthResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    const FIntRect& ViewRect)
{
    if (!SceneDepth || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
    {
        return nullptr;
    }

    const FIntPoint OutputSize = ViewRect.Size();
    const FRDGTextureDesc DepthDesc = FRDGTextureDesc::Create2D(
        OutputSize,
        PF_R32_FLOAT,
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_UAV);

    FRDGTextureRef OutputDepth = GraphBuilder.CreateTexture(DepthDesc, TEXT("DLSS5NR.DepthGuideR32F"));

    TShaderMapRef<FDLSS5NRDepthCS> ComputeShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    FDLSS5NRDepthCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDLSS5NRDepthCS::FParameters>();
    Parameters->DepthTexture = SceneDepth;
    Parameters->SourceRectMin = ViewRect.Min;
    Parameters->SourceRectSize = OutputSize;
    Parameters->OutputDepth = GraphBuilder.CreateUAV(OutputDepth);

    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("DLSS5NR Resolve Depth Guide %dx%d", OutputSize.X, OutputSize.Y),
        ComputeShader,
        Parameters,
        FComputeShaderUtils::GetGroupCount(OutputSize, FIntPoint(8, 8)));

    return OutputDepth;
}

FRDGTextureRef AddDLSS5NRMotionVectorResolvePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FRDGTextureRef SceneDepth,
    FRDGTextureRef SceneVelocity,
    const FIntRect& ViewRect)
{
    if (!SceneDepth || !SceneVelocity || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
    {
        return nullptr;
    }

    const FIntPoint OutputSize = ViewRect.Size();
    const FRDGTextureDesc MotionDesc = FRDGTextureDesc::Create2D(
        OutputSize,
        PF_G32R32F,
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_UAV);

    FRDGTextureRef OutputMotion = GraphBuilder.CreateTexture(MotionDesc, TEXT("DLSS5NR.DenseMotionVectorsRG32F"));

    // UE may bind a 1x1 black velocity texture when no object-velocity buffer exists. In that
    // case the shader reconstructs camera motion for every pixel from depth + ClipToPrevClip.
    const bool bVelocityCoversView =
        SceneVelocity->Desc.Extent.X >= ViewRect.Max.X &&
        SceneVelocity->Desc.Extent.Y >= ViewRect.Max.Y &&
        SceneVelocity->Desc.Extent.X > 1 &&
        SceneVelocity->Desc.Extent.Y > 1;

    TShaderMapRef<FDLSS5NRMotionVectorCS> ComputeShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    FDLSS5NRMotionVectorCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDLSS5NRMotionVectorCS::FParameters>();
    Parameters->View = View.ViewUniformBuffer;
    Parameters->DepthTexture = SceneDepth;
    Parameters->VelocityTexture = SceneVelocity;
    Parameters->SourceRectMin = ViewRect.Min;
    Parameters->SourceRectSize = OutputSize;
    Parameters->HasObjectVelocity = bVelocityCoversView ? 1u : 0u;
    Parameters->OutputMotionVectors = GraphBuilder.CreateUAV(OutputMotion);

    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("DLSS5NR Resolve Dense Motion Vectors %dx%d", OutputSize.X, OutputSize.Y),
        ComputeShader,
        Parameters,
        FComputeShaderUtils::GetGroupCount(OutputSize, FIntPoint(8, 8)));

    return OutputMotion;
}
