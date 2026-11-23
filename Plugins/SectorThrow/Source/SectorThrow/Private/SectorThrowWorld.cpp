#include "SectorThrowWorld.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodyInstance.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,SectorThrow)
namespace {
constexpr TCHAR Pickup[]=TEXT("/Game/Interactables/BP_PhysicsPickup.BP_PhysicsPickup_C");
constexpr TCHAR Button[]=TEXT("/Game/Interactables/AnimatedInteractables/BP_Button.BP_Button_C");
bool IsPuzzle(const AActor* A,const TCHAR* Path){for(UClass* C=A?A->GetClass():nullptr;C;C=C->GetSuperClass()) if(C->GetPathName()==Path)return true;return false;}
FVector Velocity(const APawn* P){return P->GetControlRotation().Vector()*1050.;}
const FLinearColor* Color(AActor* A,FName Key){auto* P=FindFProperty<FStructProperty>(A->GetClass(),Key);return P && P->Struct==TBaseStructure<FLinearColor>::Get()?P->ContainerPtrToValuePtr<FLinearColor>(A):nullptr;}
}
void USectorThrowWorld::Bind(APawn* Pawn){
 auto* Next=Pawn?Cast<UEnhancedInputComponent>(Pawn->InputComponent):nullptr;
 if(Input.Get()==Next)return;
 if(Input.IsValid() && Handle)Input->RemoveBindingByHandle(Handle);
 Input=Next;Handle=0;Held.Reset();Pending.Reset();
 if(!Next)return;
 // Observe the exact event already bound by the original pawn, after its Blueprint event.
 for(const auto& B:Next->GetActionEventBindings()){
  const UInputAction* A=B->GetAction();if(A && A->GetPathName()==TEXT("/Game/Inputs/Input_Toss.Input_Toss")){
   const auto Trigger=B->GetTriggerEvent();Handle=Next->BindAction(A,Trigger,this,&USectorThrowWorld::Toss).GetHandle();break;
  }
 }
}
void USectorThrowWorld::Toss(){
 auto* P=GetWorld()->GetFirstPlayerController();if(!Held.IsValid()||!P||!P->GetPawn())return;
 Pending=Held;ReleaseVelocity=Velocity(P->GetPawn());Deadline=GetWorld()->GetTimeSeconds()+1.;Held.Reset();
}
void USectorThrowWorld::Preview(UStaticMeshComponent* Cube,APawn* Pawn){
 UWorld* W=GetWorld();if(!Lines){Lines=NewObject<ULineBatchComponent>(W);Lines->RegisterComponentWithWorld(W);}
 const FVector Ext=Cube->Bounds.BoxExtent; const float Radius=FMath::Max(1.,Ext.GetMin());
 FPredictProjectilePathParams Params(Radius,Cube->Bounds.Origin,Velocity(Pawn),3.f,ECC_PhysicsBody,Cube->GetOwner());Params.ActorsToIgnore.Add(Pawn);Params.SimFrequency=30;
 FPredictProjectilePathResult R;bool Hit=UGameplayStatics::PredictProjectilePath(W,Params,R);
 const FLinearColor Blue(.13f,.75f,1);for(int32 I=1;I<R.PathData.Num();I++)Lines->DrawLine(R.PathData[I-1].Location,R.PathData[I].Location,Blue,SDPG_Foreground,4,-1);
 if(!Hit)return;
 FLinearColor Tint=Blue;bool Match=false;
 // The physical landing surface can be the floor under the button's overlap box.
 for(TActorIterator<AActor> It(W);It;++It){AActor* B=*It;if(!IsPuzzle(B,Button))continue;
  auto* Zone=B->FindComponentByClass<UBoxComponent>();if(!Zone || !Zone->Bounds.GetBox().ExpandBy(Ext).IsInsideOrOn(R.HitResult.Location))continue;
  auto* AColor=Color(Cube->GetOwner(),TEXT("PickupColor"));auto* BColor=Color(B,TEXT("PickupColor"));Match=AColor && BColor && AColor->Equals(*BColor);Tint=Match?FLinearColor::Green:FLinearColor(1,.25f,.05f);break;
 }
 const FVector Center=R.HitResult.ImpactPoint+R.HitResult.ImpactNormal*2;FVector X,Y;R.HitResult.ImpactNormal.FindBestAxisVectors(X,Y);
// Foreground depth keeps the preview readable over the held key and the gun model.
 // Ring covers the key's footprint; a short pin keeps the landing point readable from across the room.
 const float Ring=FMath::Max(Radius*1.6f,(float)Ext.Size2D());
 for(int I=0;I<32;I++){auto At=[&](int N){float A=N*2*PI/32;return Center+(X*FMath::Cos(A)+Y*FMath::Sin(A))*Ring;};Lines->DrawLine(At(I),At(I+1),Tint,SDPG_Foreground,5,-1);}
 Lines->DrawLine(Center,Center+R.HitResult.ImpactNormal*45,Tint,SDPG_Foreground,4,-1);
 if(Match){Lines->DrawLine(Center-X*Ring,Center+X*Ring,Tint,SDPG_Foreground,4,-1);Lines->DrawLine(Center-Y*Ring,Center+Y*Ring,Tint,SDPG_Foreground,4,-1);}
}
void USectorThrowWorld::Fly(UStaticMeshComponent* C){
 EndFlight();Flying=C;bFlyingNotified=C->BodyInstance.bNotifyRigidBodyCollision;FlightDeadline=GetWorld()->GetTimeSeconds()+4.;
 C->SetNotifyRigidBodyCollision(true);C->OnComponentHit.AddUniqueDynamic(this,&USectorThrowWorld::Landed);
}
void USectorThrowWorld::EndFlight(){
 if(auto* C=Flying.Get()){C->OnComponentHit.RemoveDynamic(this,&USectorThrowWorld::Landed);C->SetNotifyRigidBodyCollision(bFlyingNotified);}
 Flying.Reset();
}
void USectorThrowWorld::Landed(UPrimitiveComponent* Self,AActor*,UPrimitiveComponent*,FVector,const FHitResult& Hit){
 // Walls keep their bounce; the first floor-like contact settles the key where the preview ring showed it would land.
 if(Self!=Flying.Get() || Hit.ImpactNormal.Z<.7f)return;
 const FVector V=Self->GetPhysicsLinearVelocity();Self->SetPhysicsLinearVelocity(FVector(0,0,FMath::Min(0.,V.Z)));Self->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
 // A key that lands overlapping a pressure plate is seated on the plate centre and put to sleep. The plate's original
 // EndOverlap re-locks its door, and a key resting off-centre on the 30-unit plate drifted off it as the plate moved.
 const FVector At=Self->Bounds.Origin,Ext=Self->Bounds.BoxExtent;
 for(TActorIterator<AActor> It(GetWorld());It;++It){if(!IsPuzzle(*It,Button))continue;
  auto* Zone=It->FindComponentByClass<UBoxComponent>();if(!Zone)continue;
  const FBox Box=Zone->Bounds.GetBox();
  if(At.X<Box.Min.X-Ext.X||At.X>Box.Max.X+Ext.X||At.Y<Box.Min.Y-Ext.Y||At.Y>Box.Max.Y+Ext.Y||At.Z>Box.Max.Z+Ext.Z*2.5f)continue;
  const FVector Seat(Box.GetCenter().X,Box.GetCenter().Y,Self->GetComponentLocation().Z);
  Self->SetWorldLocationAndRotation(Seat,FRotator(0,Self->GetComponentRotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
  Self->SetPhysicsLinearVelocity(FVector::ZeroVector);Self->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Self->PutRigidBodyToSleep();
  UE_LOG(LogTemp,Display,TEXT("SectorThrow seated %s on %s at %s"),*GetNameSafe(Self->GetOwner()),*It->GetName(),*Seat.ToString());
  break;
 }
 EndFlight();
}
void USectorThrowWorld::Tick(float Dt){
 UWorld* W=GetWorld();if(Lines)Lines->Flush();auto* PC=W->GetFirstPlayerController();APawn* P=PC && PC->IsLocalController()?PC->GetPawn():nullptr;Bind(P);if(!P)return;
 if(auto* C=Pending.Get()){
  if(C->IsSimulatingPhysics() && !C->IsAttachedTo(P->GetRootComponent())){
   C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);C->SetGenerateOverlapEvents(true);C->BodyInstance.SetUseCCD(true);C->BodyInstance.ClearForces();C->SetPhysicsLinearVelocity(ReleaseVelocity);Pending.Reset();
   // The original Toss event never clears isHolding?, so the next E press was spent "dropping" a key that was already gone.
   auto* Hold=FindFProperty<FBoolProperty>(P->GetClass(),TEXT("isHolding?"));auto* Hit=FindFProperty<FObjectPropertyBase>(P->GetClass(),TEXT("HitObject"));
   if(Hold && Hit && Hit->GetObjectPropertyValue_InContainer(P)==C->GetOwner())Hold->SetPropertyValue_InContainer(P,false);
   Fly(C);
  }else if(W->GetTimeSeconds()>Deadline)Pending.Reset();
 }
 if(auto* F=Flying.Get())if(W->GetTimeSeconds()>FlightDeadline || !F->IsSimulatingPhysics())EndFlight();
 if(auto* Last=LastHeld.Get())if(Last!=Pending.Get() && Last!=Flying.Get() && Last->IsSimulatingPhysics() && !Last->IsAttachedTo(P->GetRootComponent()))Fly(Last);
 LastHeld.Reset();
 Held.Reset();TArray<USceneComponent*> Children;P->GetRootComponent()->GetChildrenComponents(true,Children);
 for(auto* Child:Children){auto* C=Cast<UStaticMeshComponent>(Child);if(C && IsPuzzle(C->GetOwner(),Pickup) && !C->IsSimulatingPhysics()){Held=C;LastHeld=C;Preview(C,P);break;}}

}
void USectorThrowWorld::Deinitialize(){EndFlight();if(Input.IsValid()&&Handle)Input->RemoveBindingByHandle(Handle);if(Lines){Lines->DestroyComponent();Lines=nullptr;}Super::Deinitialize();}
