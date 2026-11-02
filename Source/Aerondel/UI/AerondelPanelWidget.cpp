#include "UI/AerondelPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"

void UAerondelPanelButton::Setup(UAerondelPanelWidget* InOwner, int32 InIndex)
{
	Owner = InOwner; Index = InIndex;
	OnClicked.AddUniqueDynamic(this, &UAerondelPanelButton::HandleClicked);
}

void UAerondelPanelButton::HandleClicked() { if (Owner.IsValid()) Owner->RunAction(Index); }

TSharedRef<SWidget> UAerondelPanelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FLinearColor(0.06f, 0.05f, 0.04f, 0.92f));
		Frame->SetPadding(FMargin(16.f));
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Frame))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f)); S->SetAlignment(FVector2D(0.5f, 0.5f)); S->SetAutoSize(true);
		}
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size"));
		Size->SetWidthOverride(460.f); Size->SetMaxDesiredHeight(620.f);
		Frame->SetContent(Size);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
		Size->SetContent(Col);
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.35f)));
		FSlateFontInfo F = TitleText->GetFont(); F.Size = 18; TitleText->SetFont(F);
		Col->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 8));
		BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Body"));
		BodyText->SetAutoWrapText(true);
		Col->AddChildToVerticalBox(BodyText)->SetPadding(FMargin(0, 0, 0, 10));
		ButtonBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Buttons"));
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(ButtonBox)) VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	return Super::RebuildWidget();
}

void UAerondelPanelWidget::Open(FName InPanel, AActor* InContext)
{
	Panel = InPanel; Context = InContext; LiveTimer = 0.f;
	Populate(true);
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false);
		if (Buttons.Num()) Mode.SetWidgetToFocus(Buttons[0]->TakeWidget());
		PC->SetInputMode(Mode); PC->SetShowMouseCursor(true);
		if (Buttons.Num()) Buttons[0]->SetUserFocus(PC);
	}
}

void UAerondelPanelWidget::Refresh() { Populate(false); }

void UAerondelPanelWidget::Populate(bool bForceButtons)
{
	if (!TitleText) return; // tree not built yet (AddToViewport builds it before Open is called)
	FAerondelPanels::Build(GetWorld(), Panel, Context.Get(), Model);
	TitleText->SetText(FText::FromString(Model.Title));
	BodyText->SetText(FText::FromString(FString::Join(Model.Lines, TEXT("\n"))));
	BodyText->SetVisibility(Model.Lines.Num() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bForceButtons || Buttons.Num() != Model.Actions.Num())
	{
		ButtonBox->ClearChildren(); Buttons.Reset(); ButtonLabels.Reset();
		for (int32 i = 0; i < Model.Actions.Num(); ++i)
		{
			UAerondelPanelButton* B = WidgetTree->ConstructWidget<UAerondelPanelButton>(UAerondelPanelButton::StaticClass());
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			L->SetColorAndOpacity(FSlateColor(FLinearColor(0.04f, 0.04f, 0.04f)));
			B->SetContent(L); B->Setup(this, i);
			ButtonBox->AddChild(B);
			Buttons.Add(B); ButtonLabels.Add(L);
		}
		if (APlayerController* PC = GetOwningPlayer()) if (Buttons.Num()) Buttons[0]->SetUserFocus(PC);
	}
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		ButtonLabels[i]->SetText(FText::FromString(Model.Actions[i].Label));
		Buttons[i]->SetIsEnabled(Model.Actions[i].bEnabled);
	}
}

void UAerondelPanelWidget::RunAction(int32 Index)
{
	if (!Model.Actions.IsValidIndex(Index)) return;
	const bool bClose = Model.Actions[Index].bClose;
	TFunction<void()> Run = Model.Actions[Index].Run; // copy: Run may rebuild Model via events
	if (Run) Run();
	if (bClose) { OnClosed.ExecuteIfBound(); return; }
	Populate(false);
}

void UAerondelPanelWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	if (!Model.bLive) return;
	LiveTimer += Dt;
	if (LiveTimer >= 0.25f) { LiveTimer = 0.f; Populate(false); }
}

AAerondelAimMarker::AAerondelAimMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) Mesh->SetStaticMesh(Plane.Object);
}

void AAerondelAimMarker::Setup(uint8 Shape, float YawDeg, int32 Hits)
{
	// Plane is 100x100 cm. Cone/line: 4 m ahead (line narrower); circle: 6 m square under the pawn.
	const FRotator R(0.f, YawDeg, 0.f);
	if (Shape == 2) Mesh->SetWorldScale3D(FVector(6.f, 6.f, 1.f));
	else
	{
		Mesh->SetWorldScale3D(FVector(4.f, Shape == 1 ? 1.f : 3.f, 1.f));
		SetActorLocation(GetActorLocation() + R.Vector() * 200.f);
	}
	SetActorRotation(R);
	if (UMaterialInterface* Base = Mesh->GetMaterial(0))
	{
		Mat = UMaterialInstanceDynamic::Create(Base, this);
		Mat->SetVectorParameterValue(TEXT("Color"), Hits > 0 ? FLinearColor(1.f, 0.35f, 0.1f) : FLinearColor(0.9f, 0.9f, 0.6f));
		Mesh->SetMaterial(0, Mat);
	}
}

void AAerondelAimMarker::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	SetActorScale3D(GetActorScale3D() * (1.f + Dt * 0.5f));
	if (Age > 0.4f) Destroy();
}
