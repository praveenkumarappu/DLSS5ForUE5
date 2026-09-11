#pragma once

#include "CoreMinimal.h"

struct ID3D11Resource;
struct ID3D12Resource;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

using FNGXResult = uint32;
using FNGXFeature = int32;

struct FNGXHandle
{
    uint32 Id;
};

// ABI-compatible subset of NVSDK_NGX_Parameter from NVIDIA's public NGX interface.
// IMPORTANT: vtable method order must stay exactly as declared here.
struct FNGXParameter
{
    virtual void Set(const char* InName, unsigned long long InValue) = 0;
    virtual void Set(const char* InName, float InValue) = 0;
    virtual void Set(const char* InName, double InValue) = 0;
    virtual void Set(const char* InName, unsigned int InValue) = 0;
    virtual void Set(const char* InName, int InValue) = 0;
    virtual void Set(const char* InName, ID3D11Resource* InValue) = 0;
    virtual void Set(const char* InName, ID3D12Resource* InValue) = 0;
    virtual void Set(const char* InName, void* InValue) = 0;

    virtual FNGXResult Get(const char* InName, unsigned long long* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, float* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, double* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, unsigned int* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, int* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, ID3D11Resource** OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, ID3D12Resource** OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, void** OutValue) const = 0;

    virtual void Reset() = 0;
};

class FDLSS5NRParameterMap final : public FNGXParameter
{
public:
    enum class EType : uint8
    {
        U64,
        Float,
        Double,
        U32,
        I32,
        D3D11Resource,
        D3D12Resource,
        Pointer
    };

    struct FValue
    {
        EType Type = EType::U64;
        union
        {
            unsigned long long U64;
            float Float;
            double Double;
            unsigned int U32;
            int I32;
            ID3D11Resource* D3D11;
            ID3D12Resource* D3D12;
            void* Pointer;
        } Data{};
    };

    virtual void Set(const char* InName, unsigned long long InValue) override;
    virtual void Set(const char* InName, float InValue) override;
    virtual void Set(const char* InName, double InValue) override;
    virtual void Set(const char* InName, unsigned int InValue) override;
    virtual void Set(const char* InName, int InValue) override;
    virtual void Set(const char* InName, ID3D11Resource* InValue) override;
    virtual void Set(const char* InName, ID3D12Resource* InValue) override;
    virtual void Set(const char* InName, void* InValue) override;

    virtual FNGXResult Get(const char* InName, unsigned long long* OutValue) const override;
    virtual FNGXResult Get(const char* InName, float* OutValue) const override;
    virtual FNGXResult Get(const char* InName, double* OutValue) const override;
    virtual FNGXResult Get(const char* InName, unsigned int* OutValue) const override;
    virtual FNGXResult Get(const char* InName, int* OutValue) const override;
    virtual FNGXResult Get(const char* InName, ID3D11Resource** OutValue) const override;
    virtual FNGXResult Get(const char* InName, ID3D12Resource** OutValue) const override;
    virtual FNGXResult Get(const char* InName, void** OutValue) const override;

    virtual void Reset() override { Values.Reset(); }

private:
    static FName Key(const char* Name);
    template<typename T> FNGXResult Read(const char* InName, EType ExpectedType, T* OutValue) const;
    TMap<FName, FValue> Values;
};

namespace DLSS5NRNGX
{
    constexpr FNGXResult Success = 0x00000001u;
    constexpr FNGXResult FailBase = 0xBAD00000u;
    constexpr FNGXResult FailInvalidParameter = FailBase | 5u;
    constexpr FNGXResult FailNotInitialized = FailBase | 7u;
    constexpr FNGXFeature NeuralRenderingFeature = 18;

    inline bool Succeeded(FNGXResult Result)
    {
        return (Result & 0xFFF00000u) != FailBase;
    }
}
