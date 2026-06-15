

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Azr_Label.generated.h"

// Forward Declarations
class UWidgetComponent;
class UAzr_LabelWidget;

// --- STRUCTS ---
USTRUCT(BlueprintType)
struct FAzr_LabelPayload
{
	GENERATED_BODY()

	// The exact name of the Widget Component on the Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Label Payload")
	FName WidgetName = NAME_None;

	// The text you want to inject into that specific widget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Label Payload", meta = (MultiLine = true))
	FText LabelText;
};

/**
 * UAzr_Label
 * The logic manager for AzurealXR passive labels.
 * Handles visibility toggling and billboarding (always facing the player).
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Label Logic"))
class AZUREALXR_API UAzr_Label : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Label();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- CORE SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Label Settings")
	int32 InteractID = 1;

	// Add as many widgets and texts as needed here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Label Settings")
	TArray<FAzr_LabelPayload> LabelPayloads;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableLabel();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableLabel();

private:
	// --- STATE TRACKING ---
	bool bIsActive;

	// We cache the successfully found widgets so we don't have to look them up every frame on Tick
	UPROPERTY()
	TArray<UWidgetComponent*> ActiveWidgetComps;

	// --- UTILS ---
	UWidgetComponent* FindWidgetByName(FName Name);
};