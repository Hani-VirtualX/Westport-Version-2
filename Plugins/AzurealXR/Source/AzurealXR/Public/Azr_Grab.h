

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h" 
#include "CableComponent.h" 
#include "Azr_Types.h"      
#include "Haptics/HapticFeedbackEffect_Base.h" // Added for VR Haptics
#include "Azr_Grab.generated.h"

// Forward Declarations
class UAzr_Pointer;
class UAzr_AttachTarget;

// --- ENUMS ---
UENUM(BlueprintType)
enum class EAzr_HandType : uint8
{
	Left    UMETA(DisplayName = "Left Hand"),
	Right   UMETA(DisplayName = "Right Hand"),
	Both    UMETA(DisplayName = "Both Hands")
};

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGrabEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGrabAttachedEvent, int32, SequenceID, UAzr_AttachTarget*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGrabDetachedEvent, int32, SequenceID, UAzr_AttachTarget*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGrabRemoved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrabTriggerEvent, float, Value);

USTRUCT(BlueprintType)
struct FAzr_GrabConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FName TargetMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FAzr_TetherConfig TetherSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (MultiLine = true))
	FText Description;
};

USTRUCT(BlueprintType)
struct FAzr_GrabAttachConfig : public FAzr_GrabConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "false", EditConditionHides))
	int32 SequenceID = 1;
};

USTRUCT(BlueprintType)
struct FAzr_GrabRemoveConfig : public FAzr_GrabConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float RemovalDistance = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FTransform ResetTransform;
};

USTRUCT(BlueprintType)
struct FAzr_GrabTriggerConfig : public FAzr_GrabConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EAzr_HandType TriggerHand = EAzr_HandType::Both;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TriggerThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FTransform ResetTransform;
};

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_Grab : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Grab();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Settings")
	float HighlightSpeed = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Settings")
	float SnapSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Settings")
	float SnapThreshold = 3.0f;

	// --- CONFIGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration")
	FAzr_GrabConfig Grab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration")
	FAzr_GrabAttachConfig GrabAttach;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration")
	FAzr_GrabRemoveConfig GrabRemove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration")
	FAzr_GrabTriggerConfig GrabTrigger;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundHighlightEnd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundOnGrab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundOnRelease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundOnAttach;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Audio")
	USoundBase* SoundOnRemove;

	// --- HAPTICS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Haptics")
	UHapticFeedbackEffect_Base* HapticOnGrab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Haptics")
	UHapticFeedbackEffect_Base* HapticOnRelease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Haptics")
	UHapticFeedbackEffect_Base* HapticOnAttach;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Haptics")
	UHapticFeedbackEffect_Base* HapticOnRemove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Configuration|Haptics")
	UHapticFeedbackEffect_Base* HapticOnTrigger;

	// --- EVENTS ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabEvent OnGrabbed;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabEvent OnReleased;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabAttachedEvent OnGrabAttached;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabDetachedEvent OnGrabDetached;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabRemoved OnGrabRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabTriggerEvent OnGrabTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGrabTriggerEvent OnGrabTriggerReleased;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGrab();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGrabAttach(int32 SequenceID = 1);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGrabRemove();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGrabTrigger();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGrab();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGrabAttach();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGrabRemove(bool bResetTransform);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGrabTrigger(bool bResetTransform);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Settings")
	void SetTriggerHand(EAzr_HandType NewHand);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Settings")
	void SetTriggerThreshold(float NewThreshold);

	void SnapActorToHand(USceneComponent* Hand, USceneComponent* SnapPoint = nullptr);
	void ReleaseHand();
	void ForceRelease();
	void NotifyAttached(int32 ID, UAzr_AttachTarget* Target);
	void HandleTriggerInput(float Value);

private:
	UPROPERTY()
	UStaticMeshComponent* StartAnchor;
	UPROPERTY()
	UStaticMeshComponent* EndAnchor;
	UPROPERTY()
	UCableComponent* TetherCable;

	UPROPERTY()
	UMaterialParameterCollection* HighlightMPC;
	int32 StencilID;
	UPROPERTY()
	USceneComponent* CurrentHand;
	UPROPERTY()
	UPrimitiveComponent* CurrentTargetMesh;
	UPROPERTY()
	UPrimitiveComponent* AutoDetectedMesh;
	UPROPERTY()
	USceneComponent* CurrentTargetWidget;
	UPROPERTY()
	APlayerCameraManager* CachedCameraManager;

	TWeakObjectPtr<UAzr_AttachTarget> ClosestAttachTarget;
	TWeakObjectPtr<UAzr_AttachTarget> CurrentAttachedSocket;

	FTransform TargetRelativeTransform;
	bool bIsSnapping = false;
	bool bIsAttached = false;
	bool bIsGrabEnabled = false;
	bool bIsAttachMode = false;
	bool bHasTetherSettled = false;
	int32 ActiveAttachID = 0;

	bool bIsGrabRemoveMode = false;
	bool bHasTriggeredRemoval = false;
	FVector GrabRemoveStartLocation;

	bool bIsGrabTriggerMode = false;
	bool bIsTriggerPressed = false;

	float LastValue;
	bool bWasRising;

	FTimerHandle TriggerResetTimerHandle;

	void ApplyConfig(const FAzr_GrabConfig& InputConfig);
	void ToggleHighlight(bool bState, EAzr_HighlightMode Mode);
	void ToggleTether(bool bState, const FAzr_TetherConfig& TetherConfig);
	void EnsureInitialized();
	void FindAndShowGhost();
	void UpdatePointer(bool bIsGrabbing);
	bool IsHandCompatible(USceneComponent* Hand) const;
	void ExecuteTriggerReset();
	FVector CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& TetherConfig);
	USceneComponent* FindWidgetByName(FName Name);
	UPrimitiveComponent* FindMeshByName(FName Name);
	UAzr_Pointer* FindPlayerPointer() const;
};