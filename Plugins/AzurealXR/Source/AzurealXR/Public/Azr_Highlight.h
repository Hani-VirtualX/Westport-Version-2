

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Azr_Types.h" 
#include "Azr_Highlight.generated.h"

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_Highlight : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Highlight();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableHighlight();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableHighlight();

	// --- CONFIGURATION ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	FName TargetMeshName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::TargetMeshOnly;

	// --- AUDIO SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Audio")
	float HighlightSpeed = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Audio")
	USoundBase* SoundHighlightEnd;

private:
	// --- INTERNAL REFERENCES ---
	UPROPERTY()
	UMaterialParameterCollection* HighlightMPC;

	int32 StencilID;

	UPROPERTY()
	UPrimitiveComponent* CurrentTargetMesh;
	UPROPERTY()
	UPrimitiveComponent* AutoDetectedMesh;

	// Sync Logic Variables
	float LastValue;
	bool bWasRising;

	void EnsureInitialized();
	void ToggleVisuals(bool bState);
	UPrimitiveComponent* FindMeshByName(FName Name);
};