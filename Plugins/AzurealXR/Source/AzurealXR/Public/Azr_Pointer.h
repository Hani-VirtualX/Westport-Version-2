

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Azr_Pointer.generated.h"

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_Pointer : public USceneComponent
{
	GENERATED_BODY()

public:
	UAzr_Pointer();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- API ---

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic", meta = (DisplayName = "Enable Pointer (Target Component)"))
	void EnablePointer_TargetComponent(USceneComponent* Target);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic", meta = (DisplayName = "Enable Pointer (Target Location)"))
	void EnablePointer_TargetLocation(FVector TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisablePointer();

	// --- CONFIGURATION ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	float RotationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	bool bFlattenZ = false;

private:
	// Pointer Visuals (Arrow/Cone)
	UPROPERTY(VisibleAnywhere, Category = "Azureal|Visuals")
	UStaticMeshComponent* PointerMesh;

	// SAFETY FIX: Use Weak Pointer to prevent crashes on destroyed objects.
	// UPROPERTY() ensures the GC and Debugger can track this reference.
	UPROPERTY()
	TWeakObjectPtr<USceneComponent> TrackedComponent;

	FVector TrackedLocation;

	// 0=None, 1=Location, 2=Component
	uint8 TrackingMode = 0;
};