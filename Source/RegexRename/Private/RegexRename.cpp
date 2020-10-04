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
#include "Widgets/Views/SListView.h"				 // SListView

#define LOCTEXT_NAMESPACE "RegexRename"

TArray<FAssetRenameData> GetRenamedSelected(const FString& FindPattern, const FString& ReplacePattern)
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

	return RenamedAssets;
};

class SRegexRename final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRegexRename) {}
	SLATE_END_ARGS()

	using FPreviewItem = TSharedPtr<TPair<FString, FString>>;
	using FPreview = SListView<FPreviewItem>;

	TArray<FPreviewItem> PreviewItems;
	TSharedPtr<FPreview> Preview = SNew(FPreview);

	TSharedPtr<SEditableTextBox> FindPattern = SNew(SEditableTextBox);
	TSharedPtr<SEditableTextBox> ReplacePattern = SNew(SEditableTextBox);

	void UpdatePreviewItems()
	{
		TArray<FAssetRenameData> RenamedAssets =
			GetRenamedSelected(FindPattern->GetText().ToString(), ReplacePattern->GetText().ToString());

		PreviewItems.Reset();

		for (const FAssetRenameData& RenamedAsset : RenamedAssets)
		{
			PreviewItems.Add(MakeShared<TPair<FString, FString>>(RenamedAsset.Asset->GetName(), RenamedAsset.NewName));
		}

		Preview->RebuildList();
	}

	void Construct(const FArguments& InArgs, TSharedRef<SWindow>& Window)
	{
		Preview =
			SNew(FPreview)
				.ListItemsSource(&PreviewItems)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow_Lambda([](FPreviewItem Item,
										 const TSharedRef<class STableViewBase>& OwnerTable) -> TSharedRef<ITableRow> {
					return SNew(STableRow<TSharedPtr<FText>>, OwnerTable)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0, 5, 10, 5)
								[
									SNew(STextBlock).Text(FText::FromString(*Item->Key))
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0, 5, 10, 5)
								[
									SNew(STextBlock).Text(FText::FromString(*Item->Value))
								]
							];
				});

		FindPattern =
			SNew(SEditableTextBox).OnTextChanged_Lambda([this](const FText& Text) {
				UpdatePreviewItems();
			});

		ReplacePattern = 
			SNew(SEditableTextBox).OnTextChanged_Lambda([this](const FText& Text) {
				UpdatePreviewItems();
			});

		FMargin Padding{10, 10, 10, 10};

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.F)
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FindPattern", "Find pattern"))
				]
				+ SHorizontalBox::Slot()
				.Padding(Padding)
				.FillWidth(2.F)
				[
					FindPattern.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.F)
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplacePattern", "Replace pattern"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(2.F)
				.Padding(Padding)
				[
					ReplacePattern.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Preview", "Preview"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(2.F)
				.AutoWidth()
				.Padding(Padding)
				[
					Preview.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.Text(LOCTEXT("RenameButton", "Rename selected assets"))
				.OnClicked(FOnClicked::CreateLambda([this, Window]() {
					auto RenamedAssets = 
						GetRenamedSelected(FindPattern->GetText().ToString(), 
								           ReplacePattern->GetText().ToString());	

					FAssetToolsModule::GetModule().Get().RenameAssets(RenamedAssets);

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
														   .SizingRule(ESizingRule::Autosized);

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
