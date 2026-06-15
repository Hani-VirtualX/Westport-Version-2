

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Azr_Indicator.generated.h"

UCLASS()
class AZUREALXR_API AAzr_Indicator : public AActor
{
	GENERATED_BODY()

public:
	AAzr_Indicator();

protected:
	virtual void BeginPlay() override;

public:
	// --- EVENTS FOR BLUEPRINT ---
	// Implement these in your BP_Indicator to run your Timelines

	// Called automatically on spawn. Start your "Bouncing" timeline here.
	UFUNCTION(BlueprintImplementableEvent, Category = "Azureal|Indicator")
	void OnStartIdle();

	// Called when player touches the zone. Scale UP here.
	UFUNCTION(BlueprintImplementableEvent, Category = "Azureal|Indicator")
	void OnExpand();

	// Called when player leaves the zone. Scale DOWN (back to idle) here.
	UFUNCTION(BlueprintImplementableEvent, Category = "Azureal|Indicator")
	void OnShrink();
};