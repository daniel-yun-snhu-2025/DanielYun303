#include "Modules/ModuleManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"
namespace {
const FLinearColor Mint(.33f,.92f,.73f);
UObject* Ref(UObject* O,FName N) {
 auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),N);
 return P ? P->GetObjectPropertyValue_InContainer(O) : nullptr;
}
bool Flag(UObject* O,FName N) {
 auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);
 return P && P->GetPropertyValue_InContainer(O);
}
FString Number(UObject* O,FName N) {
 auto* P=FindFProperty<FNumericProperty>(O->GetClass(),N); if(!P)return TEXT("--");
 const void* V=P->ContainerPtrToValuePtr<void>(O);
 return P->IsFloatingPoint()?FString::SanitizeFloat(P->GetFloatingPointPropertyValue(V),0):LexToString(P->GetSignedIntPropertyValue(V));
}
FString Item(AActor* A) {
 if(!IsValid(A))return TEXT("Object");
 FString S=A->GetClass()->GetName(); S.RemoveFromStart(TEXT("BP_"));S.RemoveFromEnd(TEXT("_C"));
 return FName::NameToDisplayString(S.Replace(TEXT("_"),TEXT(" ")),false);
}
TSharedRef<STextBlock> Label(int Size,FLinearColor Color) {
 return SNew(STextBlock).ColorAndOpacity(Color).Font(FCoreStyle::GetDefaultFontStyle("Bold",Size));
}
TSharedRef<SWidget> Panel(TSharedRef<SWidget> Child) {
 return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.028f,.042f,.92f)).Padding(FMargin(15,8))[Child];
}
void StyleMenu(UUserWidget* W) {
 if(!W->WidgetTree)return;
 static const TMap<FString,FString> Copy={
  {TEXT("Main Menu"),TEXT("SECTOR NINE")},{TEXT("Controls"),TEXT("Controls")},
  {TEXT("Play"),TEXT("Deploy")},{TEXT("Forward : W"),TEXT("W   /   Advance")},
  {TEXT("Back : S"),TEXT("S   /   Retreat")},{TEXT("Left : A"),TEXT("A   /   Step left")},
  {TEXT("Right : D"),TEXT("D   /   Step right")},{TEXT("Space : Jump"),TEXT("SPACE   /   Jump")},
  {TEXT("Pick Up : E"),TEXT("E   /   Interact or drop")},{TEXT("Toss : R"),TEXT("R   /   Throw held item")},
  {TEXT("Left Mouse : Shoot"),TEXT("LMB   /   Fire equipped gun")}};
 W->WidgetTree->ForEachWidget([](UWidget* Widget){if(auto* T=Cast<UTextBlock>(Widget)) {
  FString Old=T->GetText().ToString(); if(const FString* New=Copy.Find(Old)) {
   T->SetText(FText::FromString(*New)); if(Old==TEXT("Main Menu"))T->SetColorAndOpacity(Mint);
  }
 }});
}
class FFieldUI : public IModuleInterface {
 FDelegateHandle Tick,Cleanup;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<UUserWidget> Menu;
 TSharedPtr<SOverlay> Root;
 TSharedPtr<STextBlock> Objective,Status,Prompt;
 TArray<TWeakObjectPtr<AActor>> Doors;
 TWeakObjectPtr<UClass> Interface;
 double Next=0;
 void Reset() {
  if(Root && World.IsValid() && World->GetGameViewport())World->GetGameViewport()->RemoveViewportWidgetContent(Root.ToSharedRef());
  Root.Reset();Status.Reset();Prompt.Reset();Objective.Reset();Doors.Reset();Menu.Reset();Interface.Reset();World.Reset();Next=0;
 }
 void Build(UWorld* W) {
  Interface=LoadClass<UInterface>(nullptr,TEXT("/Game/Interfaces/Interface_Interact.Interface_Interact_C"));
  UClass* Door=LoadClass<AActor>(nullptr,TEXT("/Game/Interactables/AnimatedInteractables/BP_Door.BP_Door_C"));
  if(Door)for(TActorIterator<AActor> I(W,Door);I;++I)Doors.Add(*I);
  Objective=Label(14,Mint);Status=Label(16,FLinearColor::White);Prompt=Label(15,Mint);
  Root=SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
   +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)[Panel(Objective.ToSharedRef())]
   +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("+")))]
   +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0,70,0,0)[Prompt.ToSharedRef()]
   +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24)[Panel(Status.ToSharedRef())];
  W->GetGameViewport()->AddViewportWidgetContent(Root.ToSharedRef(),10);
 }
 void Update(UWorld* W,ELevelTick,float) {
  if(!W || !W->IsGameWorld() || !W->GetGameViewport())return;
  APlayerController* PC=W->GetFirstPlayerController(); if(!PC || !PC->IsLocalController())return;
  if(World.Get()!=W){Reset();World=W;}
  const double Now=FPlatformTime::Seconds(); if(Now<Next)return; Next=Now+.1;
  const FString Map=W->GetMapName();
  if(Map.EndsWith(TEXT("MainMenuMap"))) {
   if(!Root){
    auto Title=Label(19,Mint);Title->SetText(FText::FromString(TEXT("SECTOR NINE   /   FIELD OPERATIONS")));
    auto Guide=Label(12,FLinearColor::White);Guide->SetText(FText::FromString(TEXT("FACILITY ACCESS     /     Mouse to look in the field")));
    Root=SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
     +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)[Panel(Title)]
     +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24)[Panel(Guide)];
    W->GetGameViewport()->AddViewportWidgetContent(Root.ToSharedRef(),10);
   }
   if(!Menu.IsValid()){TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UUserWidget::StaticClass(),true);
    for(auto* X:Widgets)if(X->GetClass()->GetName()==TEXT("MainMenu_W_C")){StyleMenu(X);Menu=X;break;}}
   return;
  }
  APawn* P=PC->GetPawn(); if(!Map.EndsWith(TEXT("TestEnvironment")) || !P){if(Root)Root->SetVisibility(EVisibility::Collapsed);return;}
  if(!Root)Build(W);
  Root->SetVisibility(EVisibility::HitTestInvisible);
  const bool Holding=Flag(P,TEXT("isHolding?"));auto* Gun=Cast<USceneComponent>(Ref(P,TEXT("gun_cube")));
  FString Hand=Holding?Item(Cast<AActor>(Ref(P,TEXT("HitObject")))):TEXT("Empty");
  const FString Ammo=FindFProperty<FNumericProperty>(P->GetClass(),TEXT("Ammo"))?Number(P,TEXT("Ammo")):(Gun && Gun->IsVisible()?TEXT("Unlimited"):TEXT("No gun"));
  Status->SetText(FText::FromString(FString::Printf(TEXT("HEALTH  %s     ARMOR  %s\nAMMO  %s     HELD  %s"),*Number(P,TEXT("Health")),*Number(P,TEXT("Amor")),*Ammo,*Hand)));
  int Total=0,Open=0;for(auto D:Doors)if(D.IsValid()){++Total;if(Flag(D.Get(),TEXT("isUnlocked")))++Open;}
  FString Task=!Total?TEXT("Explore the facility"):Open<Total?TEXT("Place the matching key on the pressure plate"):TEXT("Proceed through the unlocked door");
  FString Progress=Total?FString::Printf(TEXT("%d of %d doors unlocked"),Open,Total):TEXT("Facility");
  Objective->SetText(FText::FromString(FString::Printf(TEXT("FIELD OBJECTIVE   /   %s\n%s"),*Progress,*Task)));
  FString Hint;if(Holding)Hint=TEXT("E  Drop     R  Throw");else if(auto* Cam=Cast<USceneComponent>(Ref(P,TEXT("Camera")))) {
   FHitResult Hit;FVector Start=Cam->GetComponentLocation();FCollisionQueryParams Q(SCENE_QUERY_STAT(FieldUI),false,P);
   if(W->LineTraceSingleByChannel(Hit,Start,Start+Cam->GetForwardVector()*800.f,ECC_Visibility,Q) && Hit.GetActor() && Interface.IsValid() && Hit.GetActor()->GetClass()->ImplementsInterface(Interface.Get()))Hint=TEXT("E  Interact   /   ")+Item(Hit.GetActor());
  }
  Prompt->SetText(FText::FromString(Hint));Prompt->SetShadowOffset(FVector2D(1,1));Prompt->SetShadowColorAndOpacity(FLinearColor::Black);
 }
 public:
 void StartupModule() override {Tick=FWorldDelegates::OnWorldPostActorTick.AddRaw(this,&FFieldUI::Update);Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([this](UWorld* W,bool,bool){if(World.Get()==W)Reset();});}
 void ShutdownModule() override {FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);Reset();}
};
}
IMPLEMENT_MODULE(FFieldUI,SectorFieldUI)
