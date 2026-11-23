#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SectorDemoWorld.generated.h"

class ACharacter;
class APlayerController;
class STextBlock;
class SWidget;
class UPrimitiveComponent;

// Development-only evidence driver. It plays the real game through the player's own input
// actions (Interact E, Toss R, Fire LMB) so the GAM 495 refinements can be recorded repeatably.
// It does nothing unless the game is started with -SectorNineDemo, and the module is excluded
// from Shipping builds by its .uplugin TargetConfigurationDenyList.
UCLASS()
class USectorDemoWorld final : public UTickableWorldSubsystem {
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USectorDemoWorld, STATGROUP_Tickables); }
	virtual void Deinitialize() override;
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
private:
	void Menu(float Dt);
	void Facility(float Dt);
	bool Walk(const FVector& Target, float Dt, const TCHAR* Leg, float Tolerance = 70.f, bool bAssist = false);
	void Press(const FKey& Key, float Hold);
	void Caption(const FString& Text);
	void Note(const FString& Event, const FString& Detail = FString());
	void Shot(const FString& Name);
	void Finish(const FString& Reason);
	void Probe(const FString& Tag, const UObject* Object);
	void Track(const FString& Tag);
	void Gc(const FString& Tag);
	void Audit(const FString& Tag);
	void Look(const FVector& At);
	AActor* Aimed() const;
	UFUNCTION() void RoundHit(UPrimitiveComponent* Self, AActor* Other, UPrimitiveComponent* OtherComp, FVector Impulse, const FHitResult& Hit);
	bool Holding() const;

	APlayerController* Pc() const;
	ACharacter* Hero() const;
	AActor* Nearest(const TCHAR* ClassPath, const FVector& At) const;

	int32 Step = 0;
	float Wait = 0.f, Clock = 0.f, StepTime = 0.f;
	float StuckTimer = 0.f;
	FVector StuckFrom = FVector::ZeroVector;
	FKey HeldKey;
	float KeyLeft = 0.f;
	float Pitch = -10.f, Yaw = 0.f;
	int32 OnPlate = 0;
	int32 Shots = 0;
	int32 Waypoint = 0;
	int32 Tries = 0;
	int32 DoorTries = 0;
	TMap<FString, FString> Facts;
	TArray<FString> Assisted;
	TArray<FString> EnemyStates;
	TWeakObjectPtr<AActor> Button, Door, Door1, Enemy, Cube, Gun;
	TWeakObjectPtr<UPrimitiveComponent> CubeBody;
	FVector Gap1 = FVector::ZeroVector, Gap2 = FVector::ZeroVector;
	float TrackClock = 0.f;
	FString GcTag;
	int32 GcFrames = 0, GcCount = 0;
	float CombatGc = 0.f;
	bool bLayoutDumped = false;
	TMap<TWeakObjectPtr<AActor>, TPair<FVector, FVector>> Rounds;
	int32 RoundsLogged = 0;
	float AimLift = 0.f;
	int32 RoundsShort = 0, RoundsOnEnemy = 0;
	TSharedPtr<SWidget> Root;
	TSharedPtr<STextBlock> Label;
};
