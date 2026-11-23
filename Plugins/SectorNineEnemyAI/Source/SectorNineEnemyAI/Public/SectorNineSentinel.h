#pragma once
#include "GameFramework/Actor.h"
#include "SectorNineSentinel.generated.h"
class UBoxComponent;
UENUM(BlueprintType)
enum class ESectorNineMode : uint8 { Patrol, Alert, Attack, Search };
UCLASS(Abstract, Blueprintable)
class SECTORNINEENEMYAI_API ASectorNineSentinel : public AActor {
 GENERATED_BODY()
public:
 ASectorNineSentinel();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy AI") ESectorNineMode EnemyState=ESectorNineMode::Patrol;
 // Scale the original authored box without losing its offset or aspect ratio.
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy AI",meta=(ClampMin="0.01")) float DetectionRangeMultiplier=1.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy AI",meta=(ClampMin="1")) float ScanSpeedDegrees=45.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy AI",meta=(ClampMin="0")) float AlertSeconds=.25f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy AI",meta=(ClampMin="0")) float SearchSeconds=2.f;
private:
 UPROPERTY(Transient) UClass* PlayerType=nullptr;
 UPROPERTY(Transient) TObjectPtr<UBoxComponent> Sensor;
 UPROPERTY(Transient) TObjectPtr<USceneComponent> Pivot;
 float Elapsed=0, ShotIn=0, HomeYaw=0;
 FVector LastSeen=FVector::ZeroVector;
 void SetMode(ESectorNineMode Next);
};
