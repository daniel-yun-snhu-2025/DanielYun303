#include "SectorNineSentinel.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
ASectorNineSentinel::ASectorNineSentinel(){PrimaryActorTick.bCanEverTick=true;}
void ASectorNineSentinel::BeginPlay(){
 Super::BeginPlay();
 PlayerType=LoadClass<ACharacter>(nullptr,TEXT("/Game/Player/BP_PlayerChar.BP_PlayerChar_C"));
 TInlineComponentArray<USceneComponent*> Parts(this);
 for(auto* Part:Parts){if(Part->GetFName()==TEXT("Box")) Sensor=Cast<UBoxComponent>(Part); if(Part->GetFName()==TEXT("Pviot")) Pivot=Part;}
 if(!ensure(Sensor && Pivot)) return;
 // Retire only the original turret's timer-start/stop delegates, not variant overlap events.
 Sensor->OnComponentBeginOverlap.Remove(this,TEXT("BndEvt__BP_Turret_Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature"));
 Sensor->OnComponentEndOverlap.Remove(this,TEXT("BndEvt__BP_Turret_Box_K2Node_ComponentBoundEvent_1_ComponentEndOverlapSignature__DelegateSignature"));
 for(FName Event:{FName(TEXT("FollowPlayer")),FName(TEXT("fireAtPlayer"))}){FTimerDynamicDelegate D;D.BindUFunction(this,Event);auto H=GetWorldTimerManager().K2_FindDynamicTimerHandle(D);GetWorldTimerManager().ClearTimer(H);}
 Sensor->SetBoxExtent(Sensor->GetUnscaledBoxExtent()*FMath::Max(.01f,DetectionRangeMultiplier)); HomeYaw=Pivot->GetRelativeRotation().Yaw;
}
void ASectorNineSentinel::SetMode(ESectorNineMode Next){if(EnemyState!=Next){EnemyState=Next;Elapsed=0;}}
void ASectorNineSentinel::Tick(float Dt){
 Super::Tick(Dt); // Child Blueprint movement, drops and boss logic stay in place.
 if(!Sensor || !Pivot || !HasAuthority()) return;
 // Match the original Box -> BP_PlayerChar cast, rather than tracking a spectator.
 TArray<AActor*> Players;
 Sensor->GetOverlappingActors(Players,PlayerType);
 AActor* P=PlayerType && !Players.IsEmpty()?Players[0]:nullptr;
 const bool Seen=IsValid(P);
 if(Seen){LastSeen=P->GetActorLocation(); if(EnemyState==ESectorNineMode::Patrol || EnemyState==ESectorNineMode::Search)SetMode(ESectorNineMode::Alert);}
 else if(EnemyState==ESectorNineMode::Alert || EnemyState==ESectorNineMode::Attack)SetMode(ESectorNineMode::Search);
 Elapsed+=Dt;
 if(EnemyState==ESectorNineMode::Search && Elapsed>=SearchSeconds)SetMode(ESectorNineMode::Patrol);
 if(EnemyState==ESectorNineMode::Alert && Elapsed>=AlertSeconds){SetMode(ESectorNineMode::Attack);ShotIn=0;}
 if(EnemyState==ESectorNineMode::Patrol){auto R=Pivot->GetRelativeRotation();R.Pitch=0;R.Yaw=HomeYaw+45*FMath::Sin(Elapsed*ScanSpeedDegrees/45.f);Pivot->SetRelativeRotation(R);return;}
 auto Aim=(LastSeen-Pivot->GetComponentLocation()).Rotation();
 if(EnemyState==ESectorNineMode::Search)Aim.Yaw+=25*FMath::Sin(Elapsed*ScanSpeedDegrees/25.f);
 Pivot->SetWorldRotation(EnemyState==ESectorNineMode::Attack?Aim:FMath::RInterpConstantTo(Pivot->GetComponentRotation(),Aim,Dt,ScanSpeedDegrees));
 if(EnemyState==ESectorNineMode::Attack && (ShotIn-=Dt)<=0){
  // Use the original projectile graph and per-instance cadence, not a duplicate damage path.
  auto* Rate=FindFProperty<FDoubleProperty>(GetClass(),TEXT("FireSpeed"));ShotIn=Rate?FMath::Max(.02,Rate->GetPropertyValue_InContainer(this)):.5;
  if(auto* F=FindFunction(TEXT("fireAtPlayer")))ProcessEvent(F,nullptr);
 }
}
