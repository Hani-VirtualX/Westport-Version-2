

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h" 
#include "Azr_Types.h" 
#include "CableComponent.h"
#include "Azr_AttachTarget.generated.h"

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_AttachTarget : public USceneComponent
{
	GENERATED_BODY()

public:
	UAzr_AttachTarget();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	// --- CONFIGURATION ---

	// REQUIRED: Defines which Actor class (logic) is allowed in this socket.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	TSubclassOf<AActor> CompatibleClass;

	// OPTIONAL: If set, uses this specific mesh for the ghost. If empty, it copies the mesh from CompatibleClass.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	UStaticMesh* CustomGhostMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	int32 AttachSequenceID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	float SnapRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	float MagnetSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	FAzr_TetherConfig TetherSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals", meta = (MultiLine = true))
	FText AttachDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Visuals")
	UMaterialInterface* GhostMaterial;

	// --- STATE ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|State")
	bool bIsFilled = false;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void SetSlotFilled(bool bFilled);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void SetGhostVisibility(bool bShouldBeVisible);

	// NEW: Called by Azr_Grab when the object is pulled out (Silent Detach)
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void NotifyObjectDetached();

	void SetTetherAndWidgetVisibility(bool bShow);
	void UpdateTetherVisuals();

private:
	// --- COMPONENTS ---
	UPROPERTY()
	USphereComponent* TriggerSphere;

	UPROPERTY()
	TArray<UStaticMeshComponent*> GhostMeshes;

	UPROPERTY()
	UStaticMeshComponent* StartAnchor;
	UPROPERTY()
	UStaticMeshComponent* EndAnchor;
	UPROPERTY()
	UCableComponent* TetherCable;

	UPROPERTY()
	USceneComponent* CurrentWidget;

	// OPTIMIZATION: Cache camera to avoid finding it every Tick
	UPROPERTY()
	APlayerCameraManager* CachedCameraManager;

	// --- INTERNAL STATE ---
	bool bForceShowTether = false;
	bool bRequestedVisibility = false;

	// Magnet Animation State
	bool bIsSnapping = false;

	// SAFETY: Use Weak Pointer to prevent crashes if actor is destroyed while snapping
	TWeakObjectPtr<AActor> SnappingActor;

	FVector SnapStartLoc;
	FQuat SnapStartRot;
	float SnapAlpha;

	// Helpers
	void GenerateGhostFromClass();
	USceneComponent* FindWidgetByName(FName Name);
	FVector CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos);

	// Overlap Event
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};