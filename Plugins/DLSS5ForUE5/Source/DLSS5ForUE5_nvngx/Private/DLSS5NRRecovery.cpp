#include "DLSS5NRRecovery.h"
#include "DLSS5ForUE5Module.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"

namespace DLSS5NRRecovery
{
    bool Begin(FString& OwnedMarker)
    {
        // Editor crashes must not change a packaged game's launch configuration.
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DLSS5NR"),
            GIsEditor ? TEXT("EditorSessions") : TEXT("GameSessions"));
        if (!IFileManager::Get().MakeDirectory(*Directory, true))
        {
            UE_LOG(LogDLSS5, Warning, TEXT("Recovery marker directory is not writable: %s. Launch CVars are preserved."), *Directory);
            return false;
        }
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.lock")), true, false);
        bool bRecover = false;
        for (const FString& File : Files)
        {
            const FString Path = FPaths::Combine(Directory, File);
            FString Contents;
            TArray<FString> Fields;
            FFileHelper::LoadFileToString(Contents, *Path);
            Contents.ParseIntoArrayLines(Fields);
            uint32 Pid = 0;
            FDateTime Started;
            const bool bValid = Fields.Num() == 3 && Fields[0] == TEXT("DLSS5NRSessionV1") &&
                LexTryParseString(Pid, *Fields[1]) && Pid > 0 && FDateTime::ParseIso8601(*Fields[2],Started);
            const double Age = (FDateTime::UtcNow() - IFileManager::Get().GetTimeStamp(*Path)).GetTotalDays();
            const bool bRunning = bValid && FPlatformProcess::IsApplicationRunning(Pid);
            if (IsRecoverable(bValid, bRunning, Age)) bRecover = true;
            if (!bRunning)
            {
                // Invalid, obsolete, or consumed markers cannot repeatedly trigger recovery.
                if (!IFileManager::Get().Delete(*Path, false, true))
                    UE_LOG(LogDLSS5, Warning, TEXT("Could not remove stale recovery marker: %s"), *Path);
            }
        }
        OwnedMarker = FPaths::Combine(Directory, FString::Printf(TEXT("%u-%s.lock"),
            FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        return bRecover;
    }

    void Update(const FString& OwnedMarker)
    {
        if (OwnedMarker.IsEmpty()) return;
        const IConsoleVariable* Enable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DLSS5.Enable"));
        const bool bActive = Enable && Enable->GetInt() != 0;
        if (!bActive) { End(OwnedMarker); return; }
        if (IFileManager::Get().FileExists(*OwnedMarker)) return;
        const FString Contents = FString::Printf(TEXT("DLSS5NRSessionV1\n%u\n%s\n"),
            FPlatformProcess::GetCurrentProcessId(), *FDateTime::UtcNow().ToIso8601());
        const FString Temporary = OwnedMarker + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Contents, *Temporary) ||
            !IFileManager::Get().Move(*OwnedMarker, *Temporary, true))
        {
            // Never disable configured rendering merely because a marker write failed.
            static bool bWarned = false;
            if (!bWarned) UE_LOG(LogDLSS5, Warning, TEXT("Could not write NR session marker: %s. Crash recovery tracking unavailable."), *OwnedMarker);
            bWarned = true;
            IFileManager::Get().Delete(*Temporary, false, true);
        }
    }

    void End(const FString& OwnedMarker)
    {
        if (!OwnedMarker.IsEmpty() && IFileManager::Get().FileExists(*OwnedMarker) &&
            !IFileManager::Get().Delete(*OwnedMarker, false, true))
            UE_LOG(LogDLSS5, Warning, TEXT("Could not remove this process's recovery marker: %s"), *OwnedMarker);
    }
}
