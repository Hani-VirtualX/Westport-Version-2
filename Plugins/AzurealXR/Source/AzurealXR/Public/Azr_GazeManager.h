

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h" // Needed for FComponentReference
#include "Azr_GazeManager.generated.h"

class UAzr_Gaze;

UENUM(BlueprintType)
enum class EAzr_ManagerMode : uint8
{
	Sequential		UMETA(DisplayName = "Sequential (Follow Array Order)"),
	NonSequential	UMETA(DisplayName = "Non-Sequential (All at Once)")
};

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Gaze Manager"))
class AZUREALXR_API UAzr_GazeManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_GazeManager();

	// --- SETUP ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Management")
	EAzr_ManagerMode Mode = EAzr_ManagerMode::Sequential;

	// This creates a clean dropdown picker in the UI showing exactly your component names!
	UPROPERTY(EditAnywhere, Category = "Gaze Management")
	TArray<FComponentReference> GazeList;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Gaze Manager")
	void EnableManager();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Gaze Manager")
	void DisableManager();

private:
	int32 CurrentIndex = 0;
	int32 CompletedCount = 0;
	bool bIsManagerActive = false;

	// We store the actual, resolved component pointers here during gameplay
	UPROPERTY(Transient)
	TArray<UAzr_Gaze*> ActiveGazeZones;

	UPROPERTY(Transient)
	TArray<UAzr_Gaze*> RemainingZones;

	UFUNCTION()
	void HandleZoneTriggered(UAzr_Gaze* TriggeredZone);
};