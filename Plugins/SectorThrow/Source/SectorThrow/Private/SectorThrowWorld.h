#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SectorThrowWorld.generated.h"
class UEnhancedInputComponent;
class UInputAction;
class ULineBatchComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
UCLASS()
class USectorThrowWorld final : public UTickableWorldSubsystem {
 GENERATED_BODY()
public:
 virtual void Tick(float Dt) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USectorThrowWorld,STATGROUP_Tickables); }
 virtual void Deinitialize() override;
private:
 void Toss();
 void Bind(APawn* Pawn);
 void Preview(UStaticMeshComponent* Cube,APawn* Pawn);
 void Fly(UStaticMeshComponent* Cube);
 void EndFlight();
 UFUNCTION() void Landed(UPrimitiveComponent* Self,AActor* Other,UPrimitiveComponent* OtherComp,FVector Impulse,const FHitResult& Hit);
 TWeakObjectPtr<UEnhancedInputComponent> Input;
 TWeakObjectPtr<UStaticMeshComponent> Held,Pending,Flying,LastHeld;
 FVector ReleaseVelocity=FVector::ZeroVector;
 double Deadline=0,FlightDeadline=0;
 bool bFlyingNotified=false;
 uint32 Handle=0;
 UPROPERTY() TObjectPtr<ULineBatchComponent> Lines;
};
