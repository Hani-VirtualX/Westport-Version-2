

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Azr_Types.generated.h"


UENUM(BlueprintType)
enum class EAzr_HighlightMode : uint8
{
	None,
	TargetMeshOnly,
	AllComponents
};

UENUM(BlueprintType)
enum class EAzr_TetherPos : uint8
{
	Center,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};

// --- SHARED TETHER CONFIGURATION ---
USTRUCT(BlueprintType)
struct FAzr_TetherConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	bool bEnableTether = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether", meta = (ToolTip = "IGNORED IN EXPLAIN SYSTEM - Uses the Step's WidgetName instead."))
	FName TargetWidgetName = "TargetWidget";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	EAzr_TetherPos MeshAnchorPos = EAzr_TetherPos::Top;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	EAzr_TetherPos WidgetAnchorPos = EAzr_TetherPos::Bottom;

	// --- ADJUSTMENTS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Adjustments")
	float WidgetGap_Vertical = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Adjustments")
	float WidgetGap_Horizontal = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Adjustments")
	float MeshOffset_Vertical = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Adjustments")
	float MeshOffset_Horizontal = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Adjustments")
	float MeshSurfaceOffset = 0.0f;

	// --- VISUALS ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	class UStaticMesh* AnchorMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float AnchorScale = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float CableWidth = 35.0f;

	// FIX: Add 'class' to forward declare the type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	class UMaterialInterface* CableMaterial = nullptr;
};

// --- NEW: EXPLAIN TIMING MODE ---
UENUM(BlueprintType)
enum class EAzr_ExplainMode : uint8
{
	Audio       UMETA(DisplayName = "Sync with Audio Length"),
	CustomTimer UMETA(DisplayName = "Use Custom Timer")
};

// --- NEW: EXPLAIN SYSTEM CONFIGURATION ---
USTRUCT(BlueprintType)
struct FAzr_ExplainStep
{
	GENERATED_BODY()

	// The exact name of the UWidgetComponent attached to this Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	FName WidgetName = "ExplainWidget";

	// NEW: The written explanation text to display on the UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step", meta = (MultiLine = true))
	FText ExplainText;

	// The audio track to play when this specific widget step is active
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	class USoundBase* AudioTrack = nullptr;

	// --- NEW: TIMING LOGIC ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	EAzr_ExplainMode ExplainMode = EAzr_ExplainMode::Audio;

	// This only shows up in the editor if "Custom Timer" is selected!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step", meta = (EditCondition = "ExplainMode == EAzr_ExplainMode::CustomTimer", EditConditionHides))
	float CustomTimerDuration = 5.0f;

	// --- PER-STEP VISUALS ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step|Visuals")
	TSoftObjectPtr<AActor> ExternalTargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step|Visuals")
	FName TargetMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step|Visuals")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step|Visuals")
	float HighlightSpeed = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step|Visuals")
	FAzr_TetherConfig TetherSettings;
};