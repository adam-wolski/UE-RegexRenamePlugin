#include "CoreMinimal.h"

#include "AssetSelection.h"							 // GetSelectedAssets
#include "AssetToolsModule.h"						 // FAssetToolsModule
#include "ContentBrowserModule.h"					 // FContentBrowserModule
#include "Framework/Application/SlateApplication.h"	 // FSlateApplication
#include "Framework/MultiBox/MultiBoxBuilder.h"		 // FMenuBuilder
#include "IAssetTools.h"							 // IAssetTools
#include "Internationalization/Regex.h"				 // FRegexPatter, FRegexMatcher
#include "Modules/ModuleManager.h"					 // FModuleManager
#include "Widgets/DeclarativeSyntaxSupport.h"		 // SLATE_BEGIN_ARGS
#include "Widgets/Input/SButton.h"					 // SButton
#include "Widgets/Input/SEditableTextBox.h"			 // SEditableTextBox
#include "Widgets/SBoxPanel.h"						 // SVerticalBox
#include "Widgets/SCompoundWidget.h"				 // SCompoundWidget
#include "Widgets/SWindow.h"						 // SWindow
#include "Widgets/Text/STextBlock.h"				 // STextBlock

#define LOCTEXT_NAMESPACE "RegexRename"

void RenameSelected(const FString& FindPattern, const FString& ReplacePattern)
{
	const FRegexPattern RegexPattern{FindPattern};

	TArray<FAssetData> SelectedAssets;
	AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

	TArray<FAssetRenameData> RenamedAssets;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (!AssetData.IsUAsset())
		{
			continue;
		}

		const FString AssetName = AssetData.AssetName.ToString();
		FRegexMatcher RegexMatcher{RegexPattern, AssetName};

		if (!RegexMatcher.FindNext())
		{
			continue;
		}

		// Collect all found groups
		TArray<FStringFormatArg> CapturedGroups;
		for (int32 i = 0; RegexMatcher.GetCaptureGroupBeginning(i) != INDEX_NONE; ++i)
		{
			CapturedGroups.Add(RegexMatcher.GetCaptureGroup(i));
		}

		const FString NewAssetName = FString::Format(*ReplacePattern, CapturedGroups);

		UObject* Asset = AssetData.GetAsset();

		RenamedAssets.Add({Asset, AssetData.PackagePath.ToString(), NewAssetName});
	}

	FAssetToolsModule::GetModule().Get().RenameAssets(RenamedAssets);
};

class SRegexRename final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRegexRename) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SWindow>& Window)
	{
		TSharedRef<SEditableTextBox> FindPattern = SNew(SEditableTextBox);
		TSharedRef<SEditableTextBox> ReplacePattern = SNew(SEditableTextBox);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FindPattern", "Find pattern"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(5.F)
				[
					FindPattern
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplacePattern", "Replace pattern"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(5.F)
				[
					ReplacePattern
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("RenameButton", "Rename selected assets"))
				.OnClicked(FOnClicked::CreateLambda([FindPattern, ReplacePattern, Window]() {
					RenameSelected(FindPattern->GetText().ToString(), ReplacePattern->GetText().ToString());	
					Window->RequestDestroyWindow();
					return FReply::Handled();
				}))
			]
		];
	}
};

class FRegexRenameModule final : public IModuleInterface
{
	void StartupModule() override
	{
		FContentBrowserModule& ContentBrowserModule =
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TArray<FContentBrowserMenuExtender_SelectedAssets>& MenuExtenders =
			ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		MenuExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda(
			[](const TArray<FAssetData>& /*SelectedAssets*/) -> TSharedRef<FExtender> {
				TSharedRef<FExtender> Extender = MakeShared<FExtender>();

				auto MenuExtension = [](FMenuBuilder& MenuBuilder) {
					auto ShowRenameWindow = []() {
						TSharedRef<SWindow> RenameWindow = SNew(SWindow)
															   .Title(LOCTEXT("RenameWindow", "Regex Rename"))
															   .MinWidth(700.F)
															   .MinHeight(100.F);

						RenameWindow->SetContent(SNew(SRegexRename, RenameWindow));

						FSlateApplication::Get().AddWindow(RenameWindow);
					};

					FUIAction Action{FExecuteAction::CreateLambda(ShowRenameWindow)};

					MenuBuilder.AddMenuEntry(LOCTEXT("RegexRenameEntry", "Regex rename assets"),
											 LOCTEXT("RegexRenameEntryTooltip",
													 "Rename selected assets using regular expressions"),
											 FSlateIcon(),
											 Action);
				};

				Extender->AddMenuExtension("CommonAssetActions",
										   EExtensionHook::After,
										   nullptr,
										   FMenuExtensionDelegate::CreateLambda(MenuExtension));

				return Extender;
			}));
	}

	FContentBrowserMenuExtender_SelectedAssets MenuExtenderHandle;
};

IMPLEMENT_MODULE(FRegexRenameModule, RegexRename);

#undef LOCTEXT_NAMESPACE
