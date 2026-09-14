#include "DLSS5ForUE5EditorModule.h"

#include "DLSS5NRSettings.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DLSS5ForUE5Editor"

namespace
{
    const FName DLSS5ForUE5TabName(TEXT("DLSS5ForUE5"));
    const TCHAR* DLSS5BuildStamp = TEXT("2026-09-14");

    int32 GetPassCount()
    {
        const UDLSS5NRSettings* Settings = GetDefault<UDLSS5NRSettings>();
        return Settings ? FMath::Clamp(Settings->PassCount, 1, 4) : 1;
    }

    void SetPassCountFromSlider(float NewValue)
    {
        UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>();
        if (!Settings)
        {
            return;
        }

        const int32 NewCount = FMath::Clamp(FMath::RoundToInt(NewValue * 3.0f) + 1, 1, 4);
        if (Settings->PassCount != NewCount)
        {
            Settings->PassCount = NewCount;
            Settings->ApplyToCVars();
            Settings->SaveConfig();
            Settings->RefreshDiagnostics();
        }
    }

    class FDLSS5NRSettingsCustomization final : public IDetailCustomization
    {
    public:
        static TSharedRef<IDetailCustomization> MakeInstance()
        {
            return MakeShared<FDLSS5NRSettingsCustomization>();
        }

        virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
        {
            // Keep General at the top, then make Pass Count its own highly visible section
            // before the normal Neural Controls category.
            DetailBuilder.EditCategory(TEXT("General"), LOCTEXT("GeneralCategory", "General"), ECategoryPriority::Important);

            IDetailCategoryBuilder& PassCategory = DetailBuilder.EditCategory(
                TEXT("DLSS5PassCount"),
                LOCTEXT("PassCategory", "DLSS 5 Pass Count"),
                ECategoryPriority::Default);

            PassCategory.AddCustomRow(LOCTEXT("PassCountSearch", "DLSS 5 Pass Count Passes Multi Pass"))
            .NameContent()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PassCountLabel", "Passes"))
                .ToolTipText(LOCTEXT("PassCountTip", "Run Neural Rendering sequentially 1 to 4 times. Each additional pass consumes the previous pass output and increases GPU cost."))
            ]
            .ValueContent()
            .MinDesiredWidth(420.0f)
            .MaxDesiredWidth(1000.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SSlider)
                    .Value_Lambda([]()
                    {
                        return static_cast<float>(GetPassCount() - 1) / 3.0f;
                    })
                    .StepSize(1.0f / 3.0f)
                    .ToolTipText(LOCTEXT("PassSliderTip", "1 = one Neural Rendering pass. 4 = four sequential passes."))
                    .OnValueChanged_Lambda([](float NewValue)
                    {
                        SetPassCountFromSlider(NewValue);
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(12.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .MinDesiredWidth(22.0f)
                    .Justification(ETextJustify::Center)
                    .Text_Lambda([]()
                    {
                        return FText::AsNumber(GetPassCount());
                    })
                ]
            ];

            DetailBuilder.EditCategory(TEXT("Neural Controls"), LOCTEXT("NeuralControlsCategory", "Neural Controls"), ECategoryPriority::Default);
        }
    };

    void OpenDLSS5WindowFromConsole()
    {
        FGlobalTabmanager::Get()->TryInvokeTab(DLSS5ForUE5TabName);
    }

    FAutoConsoleCommand GDLSS5OpenWindowCommand(
        TEXT("DLSS5.Open"),
        TEXT("Open the DLSS 5 for UE5 control window."),
        FConsoleCommandDelegate::CreateStatic(&OpenDLSS5WindowFromConsole));
}

void FDLSS5ForUE5EditorModule::StartupModule()
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyEditor.RegisterCustomClassLayout(
        UDLSS5NRSettings::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FDLSS5NRSettingsCustomization::MakeInstance));
    PropertyEditor.NotifyCustomizationModuleChanged();

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DLSS5ForUE5TabName,
        FOnSpawnTab::CreateRaw(this, &FDLSS5ForUE5EditorModule::SpawnPluginTab))
        .SetDisplayName(LOCTEXT("TabTitle", "DLSS 5 for UE5"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Open DLSS 5 for UE5 controls and live diagnostics."))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDLSS5ForUE5EditorModule::RegisterMenus));

    WindowTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FDLSS5ForUE5EditorModule::TickWindow), 0.5f);
}

void FDLSS5ForUE5EditorModule::ShutdownModule()
{
    if (WindowTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(WindowTickerHandle);
        WindowTickerHandle.Reset();
    }

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DLSS5ForUE5TabName);
    DetailsView.Reset();

    if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
    {
        FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyEditor.UnregisterCustomClassLayout(UDLSS5NRSettings::StaticClass()->GetFName());
        PropertyEditor.NotifyCustomizationModuleChanged();
    }
}

void FDLSS5ForUE5EditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
    FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DLSS5ForUE5"));
    Section.AddMenuEntry(
        TEXT("OpenDLSS5ForUE5"),
        LOCTEXT("OpenWindow", "DLSS 5 for UE5"),
        LOCTEXT("OpenWindowTooltip", "Open Neural Rendering controls and live diagnostics."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FDLSS5ForUE5EditorModule::OpenWindow)));
}

void FDLSS5ForUE5EditorModule::OpenWindow()
{
    FGlobalTabmanager::Get()->TryInvokeTab(DLSS5ForUE5TabName);
}

TSharedRef<SDockTab> FDLSS5ForUE5EditorModule::SpawnPluginTab(const FSpawnTabArgs& Args)
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bAllowSearch = true;
    DetailsArgs.bHideSelectionTip = true;
    DetailsArgs.bShowOptions = true;
    DetailsArgs.bShowModifiedPropertiesOption = true;
    DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

    DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);

    if (UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>())
    {
        Settings->RefreshDiagnostics();
        DetailsView->SetObject(Settings);
    }

    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(12.0f, 10.0f, 12.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Header", "DLSS 5 for UE5"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Subtitle", "Neural Rendering controls, 3D scene guides, temporal settings, multi-pass processing, and diagnostics."))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(12.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("SafeMode", "Safe Mode"))
                    .ToolTipText(LOCTEXT("SafeModeTip", "Disable Neural Rendering and all experimental scene-guide execution, then save the safe configuration."))
                    .OnClicked_Raw(this, &FDLSS5ForUE5EditorModule::EnterSafeMode)
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(8.0f, 0.0f, 8.0f, 6.0f)
            [
                SNew(SBorder)
                .Padding(4.0f)
                [
                    DetailsView.ToSharedRef()
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8.0f, 0.0f, 8.0f, 6.0f)
            [
                SNew(SBorder)
                .Padding(8.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("LiveDiagnosticsHeader", "Live Diagnostics"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .AutoWrapText(true)
                        .Text_Lambda([]()
                        {
                            const UDLSS5NRSettings* Settings = GetDefault<UDLSS5NRSettings>();
                            if (!Settings)
                            {
                                return LOCTEXT("NoDiagnostics", "Waiting for renderer...");
                            }

                            return FText::FromString(FString::Printf(
                                TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s"),
                                *Settings->DiagnosticRuntime,
                                *Settings->DiagnosticNGX,
                                *Settings->DiagnosticGuides,
                                *Settings->DiagnosticSubrects,
                                *Settings->DiagnosticTemporal,
                                *Settings->DiagnosticCounters,
                                *Settings->DiagnosticLastResult));
                        })
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8.0f, 0.0f, 8.0f, 8.0f)
            [
                SNew(SBorder)
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("AboutHeader", "About"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .AutoWrapText(true)
                        .Text(LOCTEXT("AboutDescription", "Experimental native Unreal Engine integration for NVIDIA DLSS 3D-Guided Neural Rendering, with scene guides, temporal controls, sequential multi-pass processing, and live diagnostics."))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SSeparator)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f, 0.0f, 2.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DeveloperHeader", "About the Developer"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f, 0.0f, 6.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DeveloperName", "Praveen Kumar"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DiscordLink", "Discord"))
                            .OnClicked_Lambda([]()
                            {
                                FPlatformProcess::LaunchURL(TEXT("https://discord.com/invite/UFsuT4w"), nullptr, nullptr);
                                return FReply::Handled();
                            })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("InstagramLink", "Instagram"))
                            .OnClicked_Lambda([]()
                            {
                                FPlatformProcess::LaunchURL(TEXT("https://instagram.com/praveenkumarappu"), nullptr, nullptr);
                                return FReply::Handled();
                            })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("YouTubeLink", "YouTube"))
                            .OnClicked_Lambda([]()
                            {
                                FPlatformProcess::LaunchURL(TEXT("https://www.youtube.com/@MrPK"), nullptr, nullptr);
                                return FReply::Handled();
                            })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .ToolTipText(LOCTEXT("GitHubTip", "Open Praveen Kumar's GitHub profile."))
                            .Text(LOCTEXT("GitHubLink", "GitHub"))
                            .OnClicked_Lambda([]()
                            {
                                FPlatformProcess::LaunchURL(TEXT("https://github.com/praveenkumarappu"), nullptr, nullptr);
                                return FReply::Handled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 5.0f)
                    [
                        SNew(SSeparator)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Version: 0.6.0 Stable  |  Build: %s"), DLSS5BuildStamp)))
                    ]
                ]
            ]
        ];
}

bool FDLSS5ForUE5EditorModule::TickWindow(float DeltaTime)
{
    // Refresh only lightweight diagnostics backing strings. Never ForceRefresh() on a timer,
    // because rebuilding property widgets interrupts combo boxes, sliders, and mouse capture.
    if (UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>())
    {
        Settings->RefreshDiagnostics();
    }
    return true;
}

FReply FDLSS5ForUE5EditorModule::EnterSafeMode()
{
    if (UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>())
    {
        Settings->bEnableNeuralRendering = false;
        Settings->bEnableSceneGuides = false;
        Settings->bFeedSceneGuides = false;
        Settings->bUseMotionVectors = false;
        Settings->ApplyToCVars();
        Settings->SaveConfig();
        Settings->RefreshDiagnostics();
    }

    if (DetailsView.IsValid())
    {
        DetailsView->ForceRefresh();
    }

    return FReply::Handled();
}

IMPLEMENT_MODULE(FDLSS5ForUE5EditorModule, DLSS5ForUE5Editor)

#undef LOCTEXT_NAMESPACE