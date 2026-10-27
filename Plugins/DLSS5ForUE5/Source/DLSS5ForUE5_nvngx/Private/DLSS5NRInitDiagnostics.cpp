#include "DLSS5NRInitDiagnostics.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "DLSS5ForUE5Module.h"

#if PLATFORM_WINDOWS && DLSS5NR_WITH_D3D12
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void DLSS5NRWriteInitDiagnostic(const TCHAR* Stage, ID3D12Device* Device,
    const FNGXParameter* Parameters, const FString& RuntimePath,
    const FString& CallerPath, uint64 AppId, uint32 ApiVersion, FNGXResult Result)
{
#if PLATFORM_WINDOWS && DLSS5NR_WITH_D3D12
    if (!FParse::Param(FCommandLine::Get(), TEXT("DLSS5InitDiagnostics"))) return;
    static FCriticalSection DiagnosticMutex;
    FScopeLock DiagnosticLock(&DiagnosticMutex);
    // Initialization retries can occur every frame. Preserve the first observation
    // of each stage/result without adding per-frame disk or GPU work.
    static TSet<FString> Recorded;
    const FString Key = FString::Printf(TEXT("%s:%08X"), Stage, Result);
    if (Recorded.Contains(Key)) return;
    Recorded.Add(Key);

    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("stage"), Stage);
    Row->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
    Row->SetNumberField(TEXT("pid"), ::GetCurrentProcessId());
    wchar_t Exe[32768] = {};
    ::GetModuleFileNameW(nullptr, Exe, UE_ARRAY_COUNT(Exe));
    Row->SetStringField(TEXT("executable"), Exe);
    Row->SetStringField(TEXT("callerModule"), CallerPath);
    Row->SetStringField(TEXT("runtimePath"), FPaths::ConvertRelativePathToFull(RuntimePath));
    Row->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    Row->SetStringField(TEXT("projectDirectory"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Row->SetStringField(TEXT("appDataDirectory"), FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DLSS5NR"))));
    Row->SetStringField(TEXT("applicationId"), FString::Printf(TEXT("0x%llX"), AppId));
    Row->SetStringField(TEXT("apiVersion"), FString::Printf(TEXT("0x%X"), ApiVersion));
    Row->SetStringField(TEXT("result"), FString::Printf(TEXT("0x%08X"), Result));
    Row->SetStringField(TEXT("deviceAddress"), FString::Printf(TEXT("%p"), Device));
    Row->SetStringField(TEXT("parameterBlockAddress"), FString::Printf(TEXT("%p"), Parameters));

    TArray<TSharedPtr<FJsonValue>> Modules;
    HMODULE Handles[1024] = {};
    DWORD Bytes = 0;
    if (::K32EnumProcessModules(::GetCurrentProcess(), Handles, sizeof(Handles), &Bytes))
    {
        const uint32 Count = FMath::Min<uint32>(Bytes / sizeof(HMODULE), UE_ARRAY_COUNT(Handles));
        for (uint32 Index = 0; Index < Count; ++Index)
        {
            wchar_t Name[32768] = {};
            if (::GetModuleFileNameW(Handles[Index], Name, UE_ARRAY_COUNT(Name)))
                Modules.Add(MakeShared<FJsonValueString>(Name));
        }
        Row->SetBoolField(TEXT("moduleListTruncated"), Bytes > sizeof(Handles));
    }
    Row->SetArrayField(TEXT("loadedModules"), Modules);

    if (Parameters)
    {
        auto Values = MakeShared<FJsonObject>();
        for (const char* Name : {"SuperSampling.Available", "SuperSamplingDenoising.Available", "DLSSNR.Available", "DLSSNR.Width", "DLSSNR.Height"})
        {
            unsigned int Value = 0;
            const FNGXResult GetResult = Parameters->Get(Name, &Value);
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("getResult"), FString::Printf(TEXT("0x%08X"), GetResult));
            Entry->SetNumberField(TEXT("value"), Value);
            Values->SetObjectField(UTF8_TO_TCHAR(Name), Entry);
        }
        Row->SetObjectField(TEXT("readOnlyParameterProbes"), Values);
    }

    if (Device)
    {
        const LUID Luid = Device->GetAdapterLuid();
        Row->SetStringField(TEXT("adapterLuid"), FString::Printf(TEXT("%08X:%08X"), static_cast<uint32>(Luid.HighPart), Luid.LowPart));
        Row->SetNumberField(TEXT("deviceNodeCount"), Device->GetNodeCount());
        Row->SetStringField(TEXT("deviceRemovedReason"), FString::Printf(TEXT("0x%08X"), static_cast<uint32>(Device->GetDeviceRemovedReason())));
        ID3D12InfoQueue* Info = nullptr;
        const HRESULT Query = Device->QueryInterface(IID_PPV_ARGS(&Info));
        Row->SetStringField(TEXT("infoQueueQueryResult"), FString::Printf(TEXT("0x%08X"), static_cast<uint32>(Query)));
        if (Info)
        {
            const uint64 Count = Info->GetNumStoredMessagesAllowedByRetrievalFilter();
            Row->SetNumberField(TEXT("infoQueueMessageCount"), static_cast<double>(Count));
            TArray<TSharedPtr<FJsonValue>> Messages;
            for (uint64 Index = 0; Index < FMath::Min<uint64>(Count, 256); ++Index)
            {
                SIZE_T Size = 0;
                Info->GetMessage(Index, nullptr, &Size);
                if (!Size || Size > 1024 * 1024) continue;
                TArray<uint8> Storage; Storage.SetNumUninitialized(static_cast<int32>(Size));
                auto* Message = reinterpret_cast<D3D12_MESSAGE*>(Storage.GetData());
                if (SUCCEEDED(Info->GetMessage(Index, Message, &Size)))
                {
                    auto Entry = MakeShared<FJsonObject>();
                    Entry->SetNumberField(TEXT("id"), Message->ID);
                    Entry->SetNumberField(TEXT("severity"), Message->Severity);
                    Entry->SetStringField(TEXT("description"), UTF8_TO_TCHAR(Message->pDescription));
                    Messages.Add(MakeShared<FJsonValueObject>(Entry));
                }
            }
            Row->SetArrayField(TEXT("infoQueueMessages"), Messages);
            Row->SetBoolField(TEXT("infoQueueSnapshotTruncated"), Count > 256);
            Info->Release();
        }
    }

    FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DLSS5NR"), FString::Printf(TEXT("InitDiagnostics-%u.jsonl"), ::GetCurrentProcessId()));
    FParse::Value(FCommandLine::Get(), TEXT("DLSS5InitDiagFile="), Path);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    FString Json;
    auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Row, Writer);
    const bool bSaved = FFileHelper::SaveStringToFile(Json + LINE_TERMINATOR, *Path,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    UE_LOG(LogDLSS5, Display, TEXT("NR init diagnostic %s result=0x%08X written=%d path=%s"), Stage, Result, bSaved, *Path);
#endif
}
