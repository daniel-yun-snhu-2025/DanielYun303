#include "SectorDemoWorld.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "InputCoreTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "Containers/Ticker.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, SectorDemo)

namespace {
constexpr TCHAR PickupPath[] = TEXT("/Game/Interactables/BP_PhysicsPickup.BP_PhysicsPickup_C");
constexpr TCHAR ButtonPath[] = TEXT("/Game/Interactables/AnimatedInteractables/BP_Button.BP_Button_C");
constexpr TCHAR DoorPath[] = TEXT("/Game/Interactables/AnimatedInteractables/BP_Door.BP_Door_C");
constexpr TCHAR GunPath[] = TEXT("/Game/Interactables/BP_CollectiblePickup_Gun.BP_CollectiblePickup_Gun_C");
constexpr TCHAR EnemyPath[] = TEXT("/Game/Enemy/Turret/BP_Monster_Lv1.BP_Monster_Lv1_C");
constexpr float ThrowSpeed = 1050.f; // matches SectorThrow's release velocity

FString Dir() { return FPaths::ProjectSavedDir() / TEXT("SectorDemo"); }
FString Vec(const FVector& V) { return FString::Printf(TEXT("[%.0f,%.0f,%.0f]"), V.X, V.Y, V.Z); }
bool Flag(const UObject* O, FName N) {
	const FBoolProperty* P = O ? FindFProperty<FBoolProperty>(O->GetClass(), N) : nullptr;
	return P && P->GetPropertyValue_InContainer(O);
}
double Number(const UObject* O, FName N) {
	const FNumericProperty* P = O ? FindFProperty<FNumericProperty>(O->GetClass(), N) : nullptr;
	if (!P) return -1;
	const void* V = P->ContainerPtrToValuePtr<void>(O);
	return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(V) : double(P->GetSignedIntPropertyValue(V));
}
FString EnemyMode(const UObject* O) {
	const FEnumProperty* P = O ? FindFProperty<FEnumProperty>(O->GetClass(), TEXT("EnemyState")) : nullptr;
	if (!P) return TEXT("?");
	const int64 V = P->GetUnderlyingProperty()->GetSignedIntPropertyValue(P->ContainerPtrToValuePtr<void>(O));
	return P->GetEnum()->GetNameStringByValue(V);
}
bool IsOfPath(const AActor* A, const TCHAR* Path) {
	for (UClass* C = A ? A->GetClass() : nullptr; C; C = C->GetSuperClass()) if (C->GetPathName() == Path) return true;
	return false;
}
int32 DoorsOpen(UWorld* W, int32& Count) {
	int32 Open = 0; Count = 0;
	for (TActorIterator<AActor> It(W); It; ++It) if (IsOfPath(*It, DoorPath)) { ++Count; if (Flag(*It, TEXT("isUnlocked"))) ++Open; }
	return Open;
}
UStaticMeshComponent* HeldCube(ACharacter* P) {
	if (!P) return nullptr;
	TArray<USceneComponent*> Children; P->GetRootComponent()->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children) {
		auto* C = Cast<UStaticMeshComponent>(Child);
		if (C && IsOfPath(C->GetOwner(), PickupPath) && !C->IsSimulatingPhysics()) return C;
	}
	return nullptr;
}
FVector BodyCenter(const AActor* A) {
	// Visible mesh parts only; the large detection box would pull the aim point toward the floor.
	FBox B(ForceInit);
	if (A) for (const UActorComponent* C : A->GetComponents()) if (const auto* M = Cast<UMeshComponent>(C)) if (M->IsVisible() && M->IsCollisionEnabled()) B += M->Bounds.GetBox();
	return B.IsValid ? B.GetCenter() : (A ? A->GetActorLocation() : FVector::ZeroVector);
}
FVector Panel(const AActor* A) {
	const auto* M = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	return M ? M->Bounds.Origin : (A ? A->GetActorLocation() : FVector::ZeroVector);
}
float DoorAngle(const AActor* A) {
	const auto* Arrow = A ? A->FindComponentByClass<UArrowComponent>() : nullptr;
	return Arrow ? Arrow->GetRelativeRotation().Yaw : 0.f;
}
}

bool USectorDemoWorld::ShouldCreateSubsystem(UObject* Outer) const {
	return FParse::Param(FCommandLine::Get(), TEXT("SectorNineDemo")) && Super::ShouldCreateSubsystem(Outer);
}
bool USectorDemoWorld::DoesSupportWorldType(const EWorldType::Type WorldType) const {
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

APlayerController* USectorDemoWorld::Pc() const { return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr; }
ACharacter* USectorDemoWorld::Hero() const { APlayerController* P = Pc(); return P ? Cast<ACharacter>(P->GetPawn()) : nullptr; }
bool USectorDemoWorld::Holding() const { return HeldCube(Hero()) != nullptr; }

AActor* USectorDemoWorld::Nearest(const TCHAR* ClassPath, const FVector& At) const {
	AActor* Best = nullptr; double D = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (IsOfPath(*It, ClassPath)) { const double X = FVector::DistSquared(It->GetActorLocation(), At); if (X < D) { D = X; Best = *It; } }
	return Best;
}

void USectorDemoWorld::Note(const FString& Event, const FString& Detail) {
	const FString Line = FString::Printf(TEXT("{\"utc\":\"%s\",\"t\":%.2f,\"map\":\"%s\",\"event\":\"%s\",\"detail\":\"%s\"}\n"),
		*FDateTime::UtcNow().ToIso8601(), Clock, *GetWorld()->GetMapName(), *Event, *Detail.Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("\""), TEXT("'")));
	FFileHelper::SaveStringToFile(Line, *(Dir() / TEXT("timeline.jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	UE_LOG(LogTemp, Display, TEXT("SectorDemo %s %s"), *Event, *Detail);
}

void USectorDemoWorld::Caption(const FString& Text) {
	UGameViewportClient* V = GetWorld()->GetGameViewport();
	if (!V) return;
	if (!Root) {
		Label = SNew(STextBlock).ColorAndOpacity(FLinearColor(1.f, .86f, .3f)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 15));
		Root = SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 22, 0, 0)
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0, 0, 0, .78f)).Padding(FMargin(14, 6))[Label.ToSharedRef()]];
		V->AddViewportWidgetContent(Root.ToSharedRef(), 30);
	}
	Label->SetText(FText::FromString(TEXT("GAM 495 EVIDENCE RUN  |  ") + Text));
	Note(TEXT("caption"), Text);
}

void USectorDemoWorld::Shot(const FString& Name) {
	FScreenshotRequest::RequestScreenshot(Dir() / (Name + TEXT(".png")), true, false);
	Note(TEXT("screenshot"), Name);
}

void USectorDemoWorld::Press(const FKey& Key, float Hold) {
	if (APlayerController* P = Pc()) { P->InputKey(FInputKeyParams(Key, IE_Pressed, 1.0)); HeldKey = Key; KeyLeft = Hold; }
}

void USectorDemoWorld::Gc(const FString& Tag) {
	// Full purge at the end of this frame: a stale or corrupt UObject reference crashes here, at a known phase.
	if (!GEngine) return;
	if (FParse::Param(FCommandLine::Get(), TEXT("SectorDemoAudit"))) Audit(Tag);
	GEngine->ForceGarbageCollection(true); GcTag = Tag; GcFrames = 0; Note(TEXT("gc_request"), Tag);
}

void USectorDemoWorld::Audit(const FString& Tag) {
	// Diagnostic: hard object references (FObjectProperty, including inside containers and structs) whose value is
	// not a plausible object address. Packed late-resolve handles (low bit set) and null are skipped.
	int32 Objects = 0, Refs = 0, Bad = 0; FString Out;
	for (TObjectIterator<UObject> It; It; ++It) {
		UObject* O = *It;
		++Objects;
		for (TPropertyValueIterator<FObjectPropertyBase> P(O->GetClass(), O, EPropertyValueIteratorFlags::FullRecursion); P; ++P) {
			if (!P.Key()->IsA<FObjectProperty>()) continue;
			const UPTRINT V = *static_cast<const UPTRINT*>(P.Value()); ++Refs;
			if (V == 0 || (V & 1)) continue;
			if (V < 0x10000 || V > 0x00007FFFFFFFFFFFull || (V & 7)) {
				if (++Bad <= 30) Out += FString::Printf(TEXT("%s :: %s.%s = 0x%llx; "), *O->GetClass()->GetName(), *O->GetPathName(), *P.Key()->GetName(), uint64(V));
			}
		}
	}
	Note(TEXT("audit"), FString::Printf(TEXT("%s objects=%d object_refs=%d bad=%d %s"), *Tag, Objects, Refs, Bad, *Out));
	if (Enemy.IsValid() && !bLayoutDumped) {
		bLayoutDumped = true;
		// Every property of every class from the monster Blueprint down to AActor: owner, offset, size, type.
		for (const UStruct* K = Enemy->GetClass(); K && K != AActor::StaticClass(); K = K->GetSuperStruct()) {
			FString L = FString::Printf(TEXT("%s size=%d native=%d: "), *K->GetName(), K->GetPropertiesSize(), int32(K->IsNative()));
			for (TFieldIterator<FProperty> It(K, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				L += FString::Printf(TEXT("%s@%d+%d(%s) "), *It->GetName(), It->GetOffset_ForInternal(), It->GetSize(), *It->GetClass()->GetName());
			Note(TEXT("layout"), L);
		}
		Note(TEXT("layout"), FString::Printf(TEXT("AActor size=%d"), AActor::StaticClass()->GetPropertiesSize()));
	}
}

void USectorDemoWorld::Look(const FVector& At) {
	if (APlayerController* C = Pc()) C->SetControlRotation(UKismetMathLibrary::FindLookAtRotation(C->PlayerCameraManager->GetCameraLocation(), At));
}

AActor* USectorDemoWorld::Aimed() const {
	// Same query as the original Interact event: 800 units along the camera, Visibility channel, ignoring the player.
	APlayerController* C = Pc(); ACharacter* P = Hero();
	if (!C || !P) return nullptr;
	const FVector From = C->PlayerCameraManager->GetCameraLocation();
	FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(SectorDemoAim), false, P);
	return GetWorld()->LineTraceSingleByChannel(Hit, From, From + C->GetControlRotation().Vector() * 800.f, ECC_Visibility, Q) ? Hit.GetActor() : nullptr;
}

bool USectorDemoWorld::Walk(const FVector& Target, float Dt, const TCHAR* Leg, float Tolerance, bool bAssist) {
	ACharacter* P = Hero(); if (!P) return false;
	const FVector Here = P->GetActorLocation();
	const FVector To = FVector(Target.X - Here.X, Target.Y - Here.Y, 0);
	if (To.Size() < Tolerance) { StuckTimer = 0; return true; }
	FRotator R = Pc()->GetControlRotation(); R.Yaw = To.Rotation().Yaw; R.Pitch = FMath::FInterpTo(R.Pitch, 0.f, Dt, 4.f);
	Pc()->SetControlRotation(R);
	P->AddMovementInput(To.GetSafeNormal(), 1.f, true);
	StuckTimer += Dt;
	if (StuckTimer > 1.5f) {
		if (FVector::Dist2D(Here, StuckFrom) < 25.f) {
			Note(TEXT("blocked"), FString::Printf(TEXT("%s at %s"), Leg, *Vec(Here)));
			if (bAssist) {
				// Assisted move: recorded in the timeline and summary, never used to pass a door.
				P->SetActorLocation(FVector(Target.X, Target.Y, Here.Z), false, nullptr, ETeleportType::TeleportPhysics);
				Assisted.Add(FString::Printf(TEXT("%s->%s"), Leg, *Vec(Target)));
			}
		}
		StuckTimer = 0; StuckFrom = P->GetActorLocation();
	}
	return false;
}

void USectorDemoWorld::Tick(float Dt) {
	UWorld* W = GetWorld();
	if (!W || !W->HasBegunPlay()) return;
	Clock += Dt; StepTime += Dt;
	if (!GcTag.IsEmpty() && ++GcFrames >= 3) { Note(TEXT("gc_done"), GcTag); ++GcCount; GcTag.Empty(); }
	if (KeyLeft > 0.f && (KeyLeft -= Dt) <= 0.f) if (APlayerController* P = Pc()) P->InputKey(FInputKeyParams(HeldKey, IE_Released, 0.0));
	if (Wait > 0.f) { Wait -= Dt; return; }
	if (W->GetMapName().EndsWith(TEXT("MainMenuMap"))) Menu(Dt); else if (W->GetMapName().EndsWith(TEXT("TestEnvironment"))) Facility(Dt);
}

void USectorDemoWorld::Menu(float) {
	auto Click = [this](const TArray<FString>& Labels) -> bool {
		TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), true);
		for (UUserWidget* Wd : Widgets) {
			if (!Wd || !Wd->WidgetTree) continue;
			UButton* Found = nullptr;
			Wd->WidgetTree->ForEachWidget([&](UWidget* X) {
				if (auto* B = Cast<UButton>(X)) for (int32 I = 0; I < B->GetChildrenCount(); ++I)
					if (auto* T = Cast<UTextBlock>(B->GetChildAt(I))) if (Labels.Contains(T->GetText().ToString())) Found = B;
			});
			if (Found) { Found->OnClicked.Broadcast(); return true; }
		}
		return false;
	};
	switch (Step) {
	case 0: Note(TEXT("menu_loaded")); Caption(TEXT("1/9  Main menu: Sector Nine branding")); Wait = 3.f; ++Step; break;
	case 1: Shot(TEXT("01_menu")); Wait = 1.f; ++Step; break;
	case 2: Note(TEXT("menu_controls"), Click({TEXT("Controls")}) ? TEXT("clicked") : TEXT("not found")); Caption(TEXT("1/9  Controls screen lists the real input bindings")); Wait = 3.f; ++Step; break;
	case 3: Shot(TEXT("01b_controls")); Wait = 1.f; ++Step; break;
	case 4: Note(TEXT("menu_deploy"), Click({TEXT("Deploy"), TEXT("Play")}) ? TEXT("clicked") : TEXT("not found")); Caption(TEXT("1/9  Deploy opens the facility")); Wait = 10.f; ++Step; break;
	default: break;
	}
}

void USectorDemoWorld::Facility(float Dt) {
	UWorld* W = GetWorld();
	ACharacter* P = Hero(); APlayerController* C = Pc();
	if (!P || !C) return;
	auto Next = [this](float After = 0.f) { ++Step; Waypoint = 0; Tries = 0; StepTime = 0; Wait = After; StuckTimer = 0; StuckFrom = FVector::ZeroVector; };
	auto Goto = [this](int32 To, float After = 0.f) { Step = To; Waypoint = 0; StepTime = 0; Wait = After; StuckTimer = 0; StuckFrom = FVector::ZeroVector; };
	auto Path = [&](std::initializer_list<FVector> Points, const TCHAR* Leg, float Tol = 60.f) -> bool {
		const TArray<FVector> List(Points);
		if (Waypoint >= List.Num()) return true;
		if (Walk(List[Waypoint], Dt, Leg, Tol, false)) ++Waypoint;
		return Waypoint >= List.Num();
	};
	// Press E only when the original 800-unit Visibility trace would hit the wanted actor.
	auto InteractWith = [&](AActor* Target, const FVector& At, const TCHAR* What) -> bool {
		Look(At);
		if (StepTime < .6f || KeyLeft > 0.f) return false;
		if (Aimed() == Target) { Press(EKeys::E, .12f); Note(TEXT("press"), FString::Printf(TEXT("E on %s"), What)); return true; }
		if (StepTime > 4.f) { Note(TEXT("aim_miss"), FString::Printf(TEXT("%s: trace hit %s; target %s at %s"), What, *GetNameSafe(Aimed()), *GetNameSafe(Target), *Vec(At))); StepTime = 0.f; if (++Tries > 6) Finish(FString::Printf(TEXT("could not target %s"), What)); }
		return false;
	};
	int32 DoorTotal = 0;

	switch (Step) {
	case 0: {
		Cube = Nearest(PickupPath, FVector(3075, -2485, 335));
		CubeBody = Cube.IsValid() ? Cube->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		Button = Nearest(ButtonPath, FVector(2740, -1365, 100));
		Door1 = Nearest(DoorPath, FVector(3070, -1845, 100));
		Door = Nearest(DoorPath, FVector(3410, -370, 100));
		Gun = Nearest(GunPath, FVector(3305, -2625, 100));
		Enemy = Nearest(EnemyPath, FVector(3180, 1455, 140));
		Gap1 = Panel(Door1.Get()); Gap2 = Panel(Door.Get());
		C->SetIgnoreLookInput(true); // the driver owns the camera; a stray desktop mouse move cannot rotate the view
		Facts.Add(TEXT("health_start"), FString::SanitizeFloat(Number(P, TEXT("Health"))));
		{ const int32 Open = DoorsOpen(W, DoorTotal); Facts.Add(TEXT("doors_unlocked_start"), FString::Printf(TEXT("%d/%d"), Open, DoorTotal)); }
		Probe(TEXT("player_start"), P); Probe(TEXT("cube"), Cube.Get()); Probe(TEXT("button"), Button.Get());
		Probe(TEXT("door1"), Door1.Get()); Probe(TEXT("door2"), Door.Get()); Probe(TEXT("gun_pickup"), Gun.Get());
		Note(TEXT("facility_loaded"), FString::Printf(TEXT("cube=%d button=%d door1=%d door2=%d gun=%d enemy=%d player=%s gap1=%s gap2=%s"),
			Cube.IsValid(), Button.IsValid(), Door1.IsValid(), Door.IsValid(), Gun.IsValid(), Enemy.IsValid(), *Vec(P->GetActorLocation()), *Vec(Gap1), *Vec(Gap2)));
		Caption(TEXT("2/9  HUD: objective, health, armor, ammo and held item"));
		Gc(TEXT("facility_loaded"));
		Next(3.f); break;
	}
	case 1: Track(TEXT("key_at_start")); Shot(TEXT("02_facility_hud")); Caption(TEXT("3/9  Gun pickup: look at it, HUD shows the E prompt")); Next(.3f); break;
	case 2: if (Path({FVector(3300, -2800, 0)}, TEXT("gun"), 40.f)) Next(); break;
	case 3:
		if (!Gun.IsValid()) { Next(); break; }
		Look(Panel(Gun.Get()));
		if (StepTime > 1.f) { Shot(TEXT("03_gun_prompt")); Next(.3f); }
		break;
	case 4: if (!Gun.IsValid() || InteractWith(Gun.Get(), Panel(Gun.Get()), TEXT("gun"))) Next(1.f); break;
	case 5: {
		auto* GunMesh = Cast<USceneComponent>(FindFProperty<FObjectPropertyBase>(P->GetClass(), TEXT("gun_cube")) ? FindFProperty<FObjectPropertyBase>(P->GetClass(), TEXT("gun_cube"))->GetObjectPropertyValue_InContainer(P) : nullptr);
		const bool Has = GunMesh && GunMesh->IsVisible();
		Facts.Add(TEXT("gun_acquired"), Has ? TEXT("true") : TEXT("false"));
		Note(TEXT("gun"), FString::Printf(TEXT("gun_cube visible=%d pickup_destroyed=%d"), Has, !Gun.IsValid()));
		Shot(TEXT("03b_gun_hud")); Gc(TEXT("gun_pickup_destroyed"));
		Caption(TEXT("4/9  Physics key: look at it, HUD shows the E prompt"));
		Next(.5f); break;
	}
	case 6: if (Path({FVector(3075, -2680, 0)}, TEXT("cube"), 40.f)) Next(); break;
	case 7: if (CubeBody.IsValid()) Look(CubeBody->Bounds.Origin); if (StepTime > 1.f) { Shot(TEXT("04_interact_prompt")); Next(.2f); } break;
	case 8: if (!CubeBody.IsValid()) { Finish(TEXT("key missing")); break; } if (InteractWith(Cube.Get(), CubeBody->Bounds.Origin, TEXT("key"))) Next(.8f); break;
	case 9:
		if (Holding()) { Facts.Add(TEXT("cube_held"), TEXT("true")); Note(TEXT("cube_held"), TEXT("attached to the player")); Gc(TEXT("key_held")); Caption(TEXT("5/9  Door 1 opens with E: drop the key, open the door, pick the key up")); Next(); }
		else if (++Tries < 4) Goto(8, .4f);
		else { Facts.Add(TEXT("cube_held"), TEXT("false")); Finish(TEXT("key could not be picked up")); }
		break;
	case 10: if (Path({FVector(Gap1.X, Gap1.Y - 260, 0)}, TEXT("door1_front"), 35.f)) Next(); break;
	case 11: // E while holding drops the key (original Interact rule)
		Look(Gap1 - FVector(0, 0, 60));
		if (StepTime > .5f) { if (Holding()) { Press(EKeys::E, .12f); Note(TEXT("press"), TEXT("E drop key before door 1")); } Next(1.2f); }
		break;
	case 12: if (InteractWith(Door1.Get(), Gap1, TEXT("door 1"))) { Track(TEXT("door1_opening")); Next(.4f); } break;
	case 13:
		if (!CubeBody.IsValid()) { Finish(TEXT("key missing")); break; }
		if (InteractWith(Cube.Get(), CubeBody->Bounds.Origin, TEXT("key after door"))) Next(.5f);
		break;
	case 14:
		if (Holding()) { Note(TEXT("cube_held"), TEXT("picked up again in front of the open door")); Track(TEXT("door1_window")); Next(); }
		else if (++Tries < 3) Goto(13, .2f); else Finish(TEXT("key could not be picked up at door 1"));
		break;
	case 15: {
		if ((TrackClock += Dt) >= .25f) { TrackClock = 0.f; Track(TEXT("door1_pass")); }
		if (Path({FVector(Gap1.X, Gap1.Y + 180, 0), FVector(3010, -1690, 0)}, TEXT("through_door1"), 45.f)) {
			Facts.Add(TEXT("door1_passed"), TEXT("true")); Gc(TEXT("door1_passed"));
			Pitch = -10.f; Yaw = (Button->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw; OnPlate = 0;
			C->SetControlRotation(FRotator(Pitch, Yaw, 0));
			Caption(TEXT("6/9  Throw preview: arc + ring, green = matching pressure plate"));
			Next(.5f);
		} else if (P->GetActorLocation().Y < Gap1.Y && StepTime > 4.f) {
			Note(TEXT("door1_closed_first"), Vec(P->GetActorLocation()));
			if (++DoorTries < 3) Goto(11, .3f); else Finish(TEXT("door 1 closed before the player passed"));
		}
		break;
	}
	case 16: {
		// Closed-loop aim on the same prediction the in-game preview draws (SectorThrow: control direction * 1050).
		UStaticMeshComponent* Held = HeldCube(P);
		auto* Zone = Button.IsValid() ? Button->FindComponentByClass<UBoxComponent>() : nullptr;
		if (!Held || !Zone) { Finish(TEXT("lost held key before throw")); break; }
		const FVector Ext = Held->Bounds.BoxExtent;
		FPredictProjectilePathParams Params(FMath::Max(1., Ext.GetMin()), Held->Bounds.Origin, C->GetControlRotation().Vector() * ThrowSpeed, 3.f, ECC_PhysicsBody, Held->GetOwner());
		Params.ActorsToIgnore.Add(P); Params.SimFrequency = 30;
		FPredictProjectilePathResult R;
		const bool bHit = UGameplayStatics::PredictProjectilePath(W, Params, R);
		const FBox Box = Zone->Bounds.GetBox();
		const FVector L = R.HitResult.Location, Target = Box.GetCenter(), Cam = C->PlayerCameraManager->GetCameraLocation();
		const bool bOn = bHit && L.X >= Box.Min.X && L.X <= Box.Max.X && L.Y >= Box.Min.Y && L.Y <= Box.Max.Y;
		OnPlate = bOn ? OnPlate + 1 : 0;
		if (OnPlate >= 10) {
			Facts.Add(TEXT("preview_on_matching_plate"), TEXT("true"));
			Facts.Add(TEXT("throw_aim"), FString::Printf(TEXT("pitch %.1f yaw %.1f landing %s plate %s"), Pitch, Yaw, *Vec(L), *Vec(Target)));
			Note(TEXT("aim_found"), FString::Printf(TEXT("pitch %.1f yaw %.1f landing %s plate box %s-%s"), Pitch, Yaw, *Vec(L), *Vec(Box.Min), *Vec(Box.Max)));
			Next(1.5f); break;
		}
		if (StepTime > 12.f) { Facts.Add(TEXT("preview_on_matching_plate"), TEXT("false")); Note(TEXT("aim_not_found"), FString::Printf(TEXT("last landing %s"), *Vec(L))); Next(1.f); break; }
		if (!bOn) {
			if (!bHit) Pitch -= 1.f;
			else {
				const float Want = (Target - Cam).Rotation().Yaw, Got = (L - Cam).Rotation().Yaw;
				Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Got, Want) * .5f, -2.f, 2.f);
				Pitch += FMath::Clamp(float(FVector::Dist2D(Target, Cam) - FVector::Dist2D(L, Cam)) * .02f, -1.5f, 1.5f);
			}
			Pitch = FMath::Clamp(Pitch, -40.f, 40.f);
			C->SetControlRotation(FRotator(Pitch, Yaw, 0));
		}
		break;
	}
	case 17: Shot(TEXT("05_throw_preview")); Next(.4f); break;
	case 18:
		Track(TEXT("before_toss")); Probe(TEXT("player_before_toss"), P); TrackClock = 0.f;
		Press(EKeys::R, .12f); Note(TEXT("press"), TEXT("R toss")); Caption(TEXT("7/9  Throw: the key settles on the plate and unlocks the matching door"));
		Next(); break;
	case 19: {
		const bool Open = Door.IsValid() && Flag(Door.Get(), TEXT("isUnlocked"));
		if ((TrackClock += Dt) >= .1f) { TrackClock = 0.f; Track(TEXT("flight")); }
		const bool Settled = CubeBody.IsValid() && CubeBody->GetPhysicsLinearVelocity().Size() < 5.f && StepTime > .5f;
		if ((Open && Settled && StepTime > 1.5f) || StepTime > 6.f) {
			Facts.Add(TEXT("matching_door_unlocked"), Open ? TEXT("true") : TEXT("false"));
			Facts.Add(TEXT("key_rest"), CubeBody.IsValid() ? Vec(CubeBody->GetComponentLocation()) : TEXT("?"));
			Facts.Add(TEXT("is_holding_after_toss"), Flag(P, TEXT("isHolding?")) ? TEXT("true") : TEXT("false"));
			Note(TEXT("door2"), FString::Printf(TEXT("unlocked=%d after %.2fs key_rest=%s isHolding?=%d"), Open, StepTime, CubeBody.IsValid() ? *Vec(CubeBody->GetComponentLocation()) : TEXT("?"), Flag(P, TEXT("isHolding?"))));
			Probe(TEXT("player_after_toss"), P); Gc(TEXT("key_landed"));
			Next(.3f);
		}
		break;
	}
	case 20:
		Shot(TEXT("06_door_unlocked"));
		if (!Door.IsValid() || !Flag(Door.Get(), TEXT("isUnlocked"))) { Finish(TEXT("matching door stayed locked")); break; }
		Caption(TEXT("8/9  Door 2 is unlocked: open it with E and go to the first enemy"));
		Next(.5f); break;
	case 21: if (Path({FVector(Gap2.X, Gap2.Y - 260, 0)}, TEXT("door2_front"), 35.f)) Next(); break;
	case 22: if (InteractWith(Door.Get(), Gap2, TEXT("door 2"))) { Track(TEXT("door2_opening")); Next(.6f); } break;
	case 23:
		if ((TrackClock += Dt) >= .25f) { TrackClock = 0.f; Track(TEXT("door2_pass")); }
		if (Path({FVector(Gap2.X, Gap2.Y + 200, 0), FVector(3300, 350, 0)}, TEXT("through_door2"), 50.f)) {
			Facts.Add(TEXT("door2_passed"), TEXT("true")); Gc(TEXT("door2_passed")); CombatGc = 0.f;
			Caption(TEXT("9/9  Enemy AI: patrol -> alert -> attack; return fire with the gun"));
			Next();
		} else if (P->GetActorLocation().Y < Gap2.Y && StepTime > 5.f) {
			Note(TEXT("door2_closed_first"), Vec(P->GetActorLocation()));
			if (++DoorTries < 5) Goto(22, .3f); else Finish(TEXT("door 2 closed before the player passed"));
		}
		break;
	case 24: {
		if (!Enemy.IsValid()) { Facts.Add(TEXT("enemy_destroyed"), TEXT("true")); Note(TEXT("enemy_destroyed"), FString::Printf(TEXT("after %d shots, player health %.0f"), Shots, Number(P, TEXT("Health")))); Next(2.f); break; }
		const FString Mode = EnemyMode(Enemy.Get());
		const float Dist = FVector::Dist2D(P->GetActorLocation(), Enemy->GetActorLocation());
		if (EnemyStates.IsEmpty() || EnemyStates.Last() != Mode) { EnemyStates.Add(Mode); Note(TEXT("enemy_state"), FString::Printf(TEXT("%s dist=%.0f player_health=%.0f enemy_health=%.0f"), *Mode, Dist, Number(P, TEXT("Health")), Number(Enemy.Get(), TEXT("Health")))); }
		if ((TrackClock += Dt) >= .5f) {
			TrackClock = 0.f; int32 Theirs = 0, Ours = 0;
			for (TActorIterator<AActor> It(W); It; ++It) { if (IsOfPath(*It, TEXT("/Game/Enemy/Turret/BP_TurretProjectile.BP_TurretProjectile_C"))) ++Theirs; if (IsOfPath(*It, TEXT("/Game/Player/BP_PlayerProjectile.BP_PlayerProjectile_C"))) ++Ours; }
			Note(TEXT("combat"), FString::Printf(TEXT("state=%s dist=%.0f player_health=%.0f enemy_health=%.0f shots=%d live_enemy_projectiles=%d live_player_projectiles=%d"), *Mode, Dist, Number(P, TEXT("Health")), Number(Enemy.Get(), TEXT("Health")), Shots, Theirs, Ours));
		}
		// Projectile lifecycle: where each player round started and where it was last seen before it was destroyed.
		for (TActorIterator<AActor> It(W); It; ++It) if (IsOfPath(*It, TEXT("/Game/Player/BP_PlayerProjectile.BP_PlayerProjectile_C"))) {
			if (!Rounds.Contains(*It)) for (UActorComponent* Comp : It->GetComponents()) if (auto* Prim = Cast<UPrimitiveComponent>(Comp)) Prim->OnComponentHit.AddUniqueDynamic(this, &USectorDemoWorld::RoundHit);
			auto& R = Rounds.FindOrAdd(*It, TPair<FVector, FVector>(It->GetActorLocation(), It->GetActorLocation())); R.Value = It->GetActorLocation();
		}
		for (auto It = Rounds.CreateIterator(); It; ++It) if (!It->Key.IsValid()) {
			if (RoundsLogged++ < 12) Note(TEXT("player_round"), FString::Printf(TEXT("from %s last seen %s travelled %.0f; enemy at %s health %.0f"), *Vec(It->Value.Key), *Vec(It->Value.Value), FVector::Dist(It->Value.Key, It->Value.Value), *Vec(Enemy->GetComponentsBoundingBox().GetCenter()), Number(Enemy.Get(), TEXT("Health"))));
			It.RemoveCurrent();
		}
		if (Dist > 420.f && StepTime < 12.f) { Walk(Enemy->GetActorLocation(), Dt, TEXT("approach_enemy"), 400.f, false); break; }
		if (Number(P, TEXT("Health")) <= 0.) { Facts.Add(TEXT("enemy_destroyed"), TEXT("false")); Note(TEXT("player_down"), FString::Printf(TEXT("after %d shots, enemy health %.0f"), Shots, Number(Enemy.Get(), TEXT("Health")))); Finish(TEXT("player was defeated")); break; }
		Look(BodyCenter(Enemy.Get()) + FVector(0, 0, AimLift));
		if (!Facts.Contains(TEXT("combat_shot")) && Shots >= 3) { Facts.Add(TEXT("combat_shot"), TEXT("taken")); Shot(TEXT("07_combat")); }
		if (KeyLeft <= 0.f && FMath::Fmod(StepTime, .4f) < Dt) { Press(EKeys::LeftMouseButton, .08f); ++Shots; }
		if ((CombatGc += Dt) >= 3.f && GcTag.IsEmpty()) { CombatGc = 0.f; Gc(FString::Printf(TEXT("combat_%.0fs_state_%s_shots_%d"), StepTime, *Mode, Shots)); }
		if (StepTime > 35.f) { Facts.Add(TEXT("enemy_destroyed"), TEXT("false")); Note(TEXT("enemy_timeout"), FString::Printf(TEXT("%d shots"), Shots)); Next(1.f); }
		break;
	}
	case 25: Shot(TEXT("08_after_combat")); Next(1.f); break;
	case 26: Finish(TEXT("complete")); break;
	default: break;
	}
}

void USectorDemoWorld::RoundHit(UPrimitiveComponent* Self, AActor* Other, UPrimitiveComponent* OtherComp, FVector, const FHitResult& Hit) {
	ACharacter* P = Hero();
	if (Other && Other == Enemy.Get()) ++RoundsOnEnemy;
	else if (P && Enemy.IsValid() && FVector::Dist2D(Hit.ImpactPoint, P->GetActorLocation()) < FVector::Dist2D(Enemy->GetActorLocation(), P->GetActorLocation())) { ++RoundsShort; AimLift = FMath::Min(AimLift + 12.f, 160.f); }
	if (RoundsLogged++ < 16) Note(TEXT("round_hit"), FString::Printf(TEXT("%s.%s hit %s (%s) component %s (%s) at %s"), *GetNameSafe(Self ? Self->GetOwner() : nullptr), *GetNameSafe(Self),
		*GetNameSafe(Other), Other ? *Other->GetClass()->GetName() : TEXT("-"), *GetNameSafe(OtherComp), OtherComp ? *OtherComp->GetClass()->GetName() : TEXT("-"), *Vec(Hit.ImpactPoint)));
}

void USectorDemoWorld::Probe(const FString& Tag, const UObject* O) {
	if (!O) { Note(TEXT("probe"), Tag + TEXT(" null")); return; }
	// Blueprint-declared variables only (stops at the first native class), plus the actor's components.
	FString S = O->GetClass()->GetName() + TEXT(" vars: ");
	for (const UClass* K = O->GetClass(); K && !K->HasAnyClassFlags(CLASS_Native); K = K->GetSuperClass())
		for (TFieldIterator<FProperty> It(K, EFieldIteratorFlags::ExcludeSuper); It; ++It) {
			if (It->IsA<FMulticastDelegateProperty>()) continue;
			FString V; It->ExportTextItem_InContainer(V, O, nullptr, nullptr, PPF_None);
			if (V.Len() > 90) V = V.Left(90) + TEXT("...");
			S += FString::Printf(TEXT("%s=%s; "), *It->GetName(), *V);
		}
	if (const AActor* A = Cast<AActor>(O)) {
		S += TEXT(" | components: ");
		for (const UActorComponent* AC : A->GetComponents()) {
			S += AC->GetName() + TEXT("(") + AC->GetClass()->GetName();
			if (const auto* SC = Cast<USceneComponent>(AC)) {
				S += TEXT(" at ") + Vec(SC->GetComponentLocation()) + (SC == A->GetRootComponent() ? TEXT(" root") : TEXT(""));
				if (SC->GetAttachParent()) S += TEXT(" parent=") + SC->GetAttachParent()->GetName() + TEXT("/") + GetNameSafe(SC->GetAttachParent()->GetOwner());
			}
			if (const auto* PC2 = Cast<UPrimitiveComponent>(AC))
				S += FString::Printf(TEXT(" sim=%d col=%d overlap=%d pawn=%d physbody=%d worlddyn=%d vis=%d objtype=%d ext=%s"), PC2->IsSimulatingPhysics(), int32(PC2->GetCollisionEnabled()), PC2->GetGenerateOverlapEvents(),
					int32(PC2->GetCollisionResponseToChannel(ECC_Pawn)), int32(PC2->GetCollisionResponseToChannel(ECC_PhysicsBody)), int32(PC2->GetCollisionResponseToChannel(ECC_WorldDynamic)),
					int32(PC2->GetCollisionResponseToChannel(ECC_Visibility)), int32(PC2->GetCollisionObjectType()), *Vec(PC2->Bounds.BoxExtent));
			if (const auto* Bx = Cast<UBoxComponent>(AC)) S += TEXT(" box=") + Vec(Bx->GetScaledBoxExtent());
			if (const auto* Tl = Cast<UTimelineComponent>(AC)) S += FString::Printf(TEXT(" length=%.2fs"), Tl->GetTimelineLength());
			S += TEXT("); ");
		}
	}
	Note(TEXT("probe"), Tag + TEXT(": ") + S);
}

void USectorDemoWorld::Track(const FString& Tag) {
	UPrimitiveComponent* B = CubeBody.Get();
	ACharacter* P = Hero();
	TArray<AActor*> Over;
	if (Button.IsValid()) if (auto* Z = Button->FindComponentByClass<UBoxComponent>()) Z->GetOverlappingActors(Over);
	TArray<FString> Names; for (const AActor* A : Over) Names.Add(GetNameSafe(A));
	Note(TEXT("track"), FString::Printf(TEXT("%s player=%s key=%s sim=%d attached=%s vel=%s holding=%d door1=%d/%.0fdeg door2=%d/%.0fdeg plate=[%s]"), *Tag,
		P ? *Vec(P->GetActorLocation()) : TEXT("?"),
		B ? *Vec(B->GetComponentLocation()) : TEXT("?"), B ? int32(B->IsSimulatingPhysics()) : -1,
		B && B->GetAttachParent() ? *GetNameSafe(B->GetAttachParent()->GetOwner()) : TEXT("none"),
		B ? *Vec(B->GetPhysicsLinearVelocity()) : TEXT("?"), P ? int32(Flag(P, TEXT("isHolding?"))) : -1,
		Door1.IsValid() ? int32(Flag(Door1.Get(), TEXT("isUnlocked"))) : -1, DoorAngle(Door1.Get()),
		Door.IsValid() ? int32(Flag(Door.Get(), TEXT("isUnlocked"))) : -1, DoorAngle(Door.Get()), *FString::Join(Names, TEXT(","))));
}

void USectorDemoWorld::Finish(const FString& Reason) {
	UWorld* W = GetWorld(); ACharacter* P = Hero(); int32 DoorTotal = 0;
	Facts.Add(TEXT("result"), Reason);
	Facts.Add(TEXT("health_end"), P ? FString::SanitizeFloat(Number(P, TEXT("Health"))) : TEXT("?"));
	{ const int32 Open = DoorsOpen(W, DoorTotal); Facts.Add(TEXT("doors_unlocked_end"), FString::Printf(TEXT("%d/%d"), Open, DoorTotal)); }
	Facts.Add(TEXT("enemy_states"), FString::Join(EnemyStates, TEXT(">")));
	Facts.Add(TEXT("shots_fired"), FString::FromInt(Shots));
	Facts.Add(TEXT("rounds_on_enemy"), FString::FromInt(RoundsOnEnemy));
	Facts.Add(TEXT("rounds_short"), FString::FromInt(RoundsShort));
	Facts.Add(TEXT("assisted_moves"), FString::Join(Assisted, TEXT(";")));
	Facts.Add(TEXT("forced_gc_survived"), FString::FromInt(GcCount));
	FString Json = TEXT("{\n");
	for (const auto& KV : Facts) Json += FString::Printf(TEXT("  \"%s\": \"%s\",\n"), *KV.Key, *KV.Value.Replace(TEXT("\""), TEXT("'")));
	Json += FString::Printf(TEXT("  \"finished_utc\": \"%s\"\n}\n"), *FDateTime::UtcNow().ToIso8601());
	FFileHelper::SaveStringToFile(Json, *(Dir() / TEXT("summary.json")));
	Note(TEXT("finish"), Reason);
	Caption(TEXT("Evidence run finished: ") + Reason);
	if (APlayerController* C = Pc()) C->ResetIgnoreLookInput();

	Step = 1000; Wait = 1e9f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExit(false); return false; }), 3.f);
}

void USectorDemoWorld::Deinitialize() {
	if (Root && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Root.ToSharedRef());
	Root.Reset(); Label.Reset();
	Super::Deinitialize();
}
