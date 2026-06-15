

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Actor.h" 
#include "Azr_Teleport.generated.h"

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_Teleport : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Teleport();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Azureal|Teleport")
	void HandleTeleportInput(float InputValue, USceneComponent* ControllerSource);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Teleport")
	void TeleportToLocation(FVector TargetLocation);

	// --- STATE API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Teleport")
	void EnableTeleport();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Teleport")
	void DisableTeleport();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Azureal|Settings")
	float MaxTeleportDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Azureal|Settings")
	float FadeDuration = 0.15f;

	// -- VISUALS (RETICLE) --
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UStaticMesh* ReticleMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	FVector ReticleScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UMaterialInterface* ValidReticleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UMaterialInterface* InvalidReticleMaterial;

	// -- VISUALS (BEAM) --
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UStaticMesh* BeamMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	FVector BeamScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UMaterialInterface* ValidBeamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UMaterialInterface* InvalidBeamMaterial;

private:
	// --- STATE TRACKERS ---
	bool bIsTeleportEnabled = true;
	bool bIsAiming;
	bool bHasValidLocation;
	FVector ValidTeleportLocation;

	FTimerHandle TeleportTimerHandle;

	UPROPERTY()
	UStaticMeshComponent* ReticleComponent;

	UPROPERTY()
	USceneComponent* CurrentController;

	UPROPERTY()
	USplineComponent* ArcSpline;

	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshPool;

	void EnsureInitialized();

	void StartAiming();
	void StopAiming(bool bExecuteTeleport);
	void UpdateArc();
	void PerformTeleport();
	void DrawBeam(const TArray<FVector>& PathPoints);
	void UpdateVisualsColor();
};