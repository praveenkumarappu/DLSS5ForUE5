#include "DLSS5NRNGXABI.h"

#include <type_traits>

FName FDLSS5NRParameterMap::Key(const char* Name)
{
    return Name ? FName(UTF8_TO_TCHAR(Name)) : NAME_None;
}

template<typename T>
FNGXResult FDLSS5NRParameterMap::Read(const char* InName, EType ExpectedType, T* OutValue) const
{
    if (!InName || !OutValue)
    {
        return DLSS5NRNGX::FailInvalidParameter;
    }

    const FValue* V = Values.Find(Key(InName));
    if (!V || V->Type != ExpectedType)
    {
        return DLSS5NRNGX::FailInvalidParameter;
    }

    if constexpr (std::is_same_v<T, unsigned long long>) *OutValue = V->Data.U64;
    else if constexpr (std::is_same_v<T, float>) *OutValue = V->Data.Float;
    else if constexpr (std::is_same_v<T, double>) *OutValue = V->Data.Double;
    else if constexpr (std::is_same_v<T, unsigned int>) *OutValue = V->Data.U32;
    else if constexpr (std::is_same_v<T, int>) *OutValue = V->Data.I32;
    else if constexpr (std::is_same_v<T, ID3D11Resource*>) *OutValue = V->Data.D3D11;
    else if constexpr (std::is_same_v<T, ID3D12Resource*>) *OutValue = V->Data.D3D12;
    else if constexpr (std::is_same_v<T, void*>) *OutValue = V->Data.Pointer;
    else return DLSS5NRNGX::FailInvalidParameter;

    return DLSS5NRNGX::Success;
}

void FDLSS5NRParameterMap::Set(const char* N, unsigned long long X) { FValue V; V.Type=EType::U64; V.Data.U64=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, float X)              { FValue V; V.Type=EType::Float; V.Data.Float=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, double X)             { FValue V; V.Type=EType::Double; V.Data.Double=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, unsigned int X)       { FValue V; V.Type=EType::U32; V.Data.U32=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, int X)                { FValue V; V.Type=EType::I32; V.Data.I32=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, ID3D11Resource* X)    { FValue V; V.Type=EType::D3D11Resource; V.Data.D3D11=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, ID3D12Resource* X)    { FValue V; V.Type=EType::D3D12Resource; V.Data.D3D12=X; Values.Add(Key(N),V); }
void FDLSS5NRParameterMap::Set(const char* N, void* X)              { FValue V; V.Type=EType::Pointer; V.Data.Pointer=X; Values.Add(Key(N),V); }

FNGXResult FDLSS5NRParameterMap::Get(const char* N, unsigned long long* X) const { return Read(N,EType::U64,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, float* X) const              { return Read(N,EType::Float,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, double* X) const             { return Read(N,EType::Double,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, unsigned int* X) const       { return Read(N,EType::U32,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, int* X) const                { return Read(N,EType::I32,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, ID3D11Resource** X) const    { return Read(N,EType::D3D11Resource,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, ID3D12Resource** X) const    { return Read(N,EType::D3D12Resource,X); }
FNGXResult FDLSS5NRParameterMap::Get(const char* N, void** X) const              { return Read(N,EType::Pointer,X); }
