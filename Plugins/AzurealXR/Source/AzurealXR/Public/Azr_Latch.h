

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialParameterCollection.h" 
#include "Azr_Types.h"
#include "Haptics/HapticFeedbackEffect_Base.h" 
#include "Azr_Latch.generated.h"

// Forward Declarations
class UCableComponent;
class UAzr_Pointer;
class UAzr_LatchZone;

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLatchEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLatchUpdateEvent, float, Value);

// --- ENUMS ---
UENUM(BlueprintType)
enum class EAzr_LatchType : uint8
{
	Angular		UMETA(DisplayName = "Angular (Lever / Door)"),
	Linear		UMETA(DisplayName = "Linear (Slider / Drawer / Bolt)"),
	Rotation	UMETA(DisplayName = "Rotation (Valve / Doorknob)")
};

UENUM(BlueprintType)
enum class EAzr_Axis : uint8
{
	X_Axis		UMETA(DisplayName = "X Axis (Red)"),
	Y_Axis		UMETA(DisplayName = "Y Axis (Green)"),
	Z_Axis		UMETA(DisplayName = "Z Axis (Blue)")
};

UENUM(BlueprintType)
enum class EAzr_LatchTrigger : uint8
{
	Disabled	UMETA(DisplayName = "Disabled"),
	LeftHand	UMETA(DisplayName = "Left Hand"),
	RightHand	UMETA(DisplayName = "Right Hand"),
	BothHands	UMETA(DisplayName = "Both Hands")
};

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_Latch : public USceneComponent
{
	GENERATED_BODY()

public:
	UAzr_Latch();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- THE MODULAR ID ---
	// Matches the InteractID on the Azr_LatchZone and Azr_LatchSnap to establish a link.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	int32 InteractID = 1;

	// --- CONFIGURATION ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	EAzr_LatchType LatchType = EAzr_LatchType::Angular;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	EAzr_Axis InteractionAxis = EAzr_Axis::X_Axis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	float StartValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	float LimitMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	float LimitMax = 90.0f;

	// --- TRIGGER SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	EAzr_LatchTrigger LatchTrigger = EAzr_LatchTrigger::Disabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	float TriggerThreshold = 0.5f;

	// Auto Release Toggle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	bool bAutoRelease = true;

	// --- AUTO RETURN ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	bool bUseAutoReturn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration", meta = (EditCondition = "bUseAutoReturn"))
	float ReturnValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration", meta = (EditCondition = "bUseAutoReturn"))
	float ReturnSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration")
	float Damping = 15.0f;

	// --- SETUP ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	FName HandleMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	bool bUseLatchSnapHand = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	float MinLeverArmLength = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	float MaxBreakDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	float HandFlyDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	FAzr_TetherConfig TetherSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup", meta = (MultiLine = true))
	FText LatchDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Setup")
	float HighlightSpeed = 0.8f;

	// --- HAPTICS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Haptics")
	bool bHapticOnStep = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Haptics", meta = (EditCondition = "bHapticOnStep"))
	float StepSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Haptics", meta = (EditCondition = "bHapticOnStep"))
	UHapticFeedbackEffect_Base* HapticEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Haptics", meta = (EditCondition = "bHapticOnStep"))
	float HapticIntensity = 1.5f;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Audio")
	USoundBase* SoundHighlightEnd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Audio")
	USoundBase* SoundOnLatch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Configuration|Audio")
	USoundBase* SoundOnRelease;

	// --- EVENTS ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnLatchUpdateEvent OnValueChanged;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnLatchUpdateEvent OnRawValueChanged;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnLatchEvent OnLatched;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnLatchEvent OnLatchReleased;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableLatch();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableLatch();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void ForceLatchRelease();

	
	// Use this to safely change math rules at runtime without breaking the handle's pivot point.
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void SetLatchConfig(EAzr_LatchType NewType, EAzr_Axis NewAxis, float NewMin, float NewMax, float NewStartValue);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void HandleTriggerInput(float Value);

	void GrabLatch(USceneComponent* Hand);
	void ReleaseLatch();
	void ReleaseSpecificHand(USceneComponent* Hand);

private:
	// --- INTERNAL COMPONENTS ---
	UPROPERTY()
	UStaticMeshComponent* StartAnchor;
	UPROPERTY()
	UStaticMeshComponent* EndAnchor;
	UPROPERTY()
	UCableComponent* TetherCable;

	// --- HIGHLIGHT ENGINE ---
	UPROPERTY()
	UMaterialParameterCollection* HighlightMPC;
	int32 StencilID;
	float LastValue;
	bool bWasRising;

	// --- STATE ---
	bool bInvertRotation = false;
	bool bIsGrabEnabled = false;
	bool bIsGrabbed = false;
	bool bIsTriggerEngaged = false;
	bool bHasTetherSettled = false;

	UPROPERTY()
	TArray<USceneComponent*> ActiveHands;

	UPROPERTY()
	TArray<USceneComponent*> ActiveGhostHands;

	UPROPERTY()
	UPrimitiveComponent* TargetHandleMesh;

	UPROPERTY()
	UPrimitiveComponent* AutoDetectedMesh;

	UPROPERTY()
	USceneComponent* CurrentTargetWidget;

	UPROPERTY(Transient)
	UAzr_LatchZone* LinkedLatchZone;

	UPROPERTY()
	APawn* CachedPlayerPawn;

	float CurrentRawValue = 0.0f;
	float GrabOffsetValue = 0.0f;
	float LastHapticValue = -999.0f;
	FTransform InitialTransform;

	float LastAngle = 0.0f;
	FVector TrackedLocalHandAxis = FVector::UpVector;

	bool bIsHandFlying = false;
	float HandFlyTime = 0.0f;
	FTransform HandFlyStartRelative;
	FTransform HandFlyTargetRelative;

	FTransform IdealLeftGripTransform;
	FTransform IdealRightGripTransform;

	UPROPERTY()
	USceneComponent* CustomLeftSnap;

	UPROPERTY()
	USceneComponent* CustomRightSnap;

	// --- HELPERS ---
	void EnsureInitialized();
	void AutoDetectLatchZone();
	void ToggleTether(bool bState);
	void ToggleHighlight(bool bState);
	void UpdatePointer(bool bIsGrabbing);
	FVector CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& TetherConfig);

	UPrimitiveComponent* FindMeshByName(FName Name);
	USceneComponent* FindWidgetByName(FName Name);
	UAzr_Pointer* FindPlayerPointer() const;
	FVector GetAxisVector() const;
	float CalculateAngle(FVector HandPos) const;
	float CalculateLinearDist(FVector HandPos) const;
	float CalculateTwistAngle(USceneComponent* Hand) const;
	void HandleAngularInteraction(float DeltaTime);
	void HandleLinearInteraction(float DeltaTime);
	void HandleRotationInteraction(float DeltaTime);
	void HandleAutoReturn(float DeltaTime);
	void HandleHandFly(float DeltaTime);
	void CheckBreakDistance();
	void ApplyConstraints(float PrePhysicsRawValue);
};