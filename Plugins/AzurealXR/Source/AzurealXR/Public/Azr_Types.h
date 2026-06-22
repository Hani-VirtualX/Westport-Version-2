

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


	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float CableHang = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	class UMaterialInterface* CableMaterial = nullptr;
};


UENUM(BlueprintType)
enum class EAzr_ExplainMode : uint8
{
	Audio       UMETA(DisplayName = "Sync with Audio Length"),
	CustomTimer UMETA(DisplayName = "Use Custom Timer")
};


USTRUCT(BlueprintType)
struct FAzr_ExplainStep
{
	GENERATED_BODY()

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	FName WidgetName = "ExplainWidget";

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step", meta = (MultiLine = true))
	FText ExplainText;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	class USoundBase* AudioTrack = nullptr;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step")
	EAzr_ExplainMode ExplainMode = EAzr_ExplainMode::Audio;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Step", meta = (EditCondition = "ExplainMode == EAzr_ExplainMode::CustomTimer", EditConditionHides))
	float CustomTimerDuration = 5.0f;

	

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