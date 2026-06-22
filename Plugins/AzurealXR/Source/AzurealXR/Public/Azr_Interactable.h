



#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h" 
#include "Azr_Interactable.generated.h"

class UAzr_Pointer;
class AAzr_Indicator;

UCLASS()
class AZUREALXR_API AAzr_Interactable : public AActor
{
	GENERATED_BODY()

public:
	AAzr_Interactable();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	// --- SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Settings")
	bool bForceDisableShadows = true;

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
	UBoxComponent* GrabZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
	UStaticMeshComponent* LeftSnap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
	UStaticMeshComponent* RightSnap;

	// --- VISUALS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	TSubclassOf<AAzr_Indicator> IndicatorClass;

	UPROPERTY()
	AAzr_Indicator* SpawnedIndicator;

	// --- HIVE MIND (GLOBAL SYNC) ---
	// Static variable shared by ALL instances of this class in the game.
	static float GlobalHiveSpeed;

	UFUNCTION(BlueprintCallable, Category = "Azureal|HiveMind")
	static void SetGlobalHiveSpeed(float NewSpeed);

	UFUNCTION(BlueprintPure, Category = "Azureal|HiveMind")
	static float GetGlobalHiveSpeed();

	// --- SHORTCUTS ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Helper")
	void EnablePointer(USceneComponent* Target = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Helper")
	void DisablePointer();

	UFUNCTION(BlueprintPure, Category = "Azureal|Interaction")
	USceneComponent* GetSnapPoint(bool bIsRightHand) const;

private:
	UAzr_Pointer* FindPlayerPointer();

	UPROPERTY()
	APawn* CachedPlayerPawn;

	FTimerHandle PointerDelayHandle;

	UFUNCTION()
	void OnGrabZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnGrabZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};