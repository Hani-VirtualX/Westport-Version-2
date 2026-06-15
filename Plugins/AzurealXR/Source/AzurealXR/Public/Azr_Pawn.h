

#pragma once

#include "Azr_HandScanner.h"
#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MotionControllerComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Azr_Pawn.generated.h"

class UInputMappingContext;
class UInputAction;
class UAzr_Teleport;
class UAzr_HandAnimInstance;
class UAzr_Gaze;
class UWidgetComponent;
class UAzr_ExplainWidget;

// --- NEW: LOCOMOTION HAND SETTING ---
UENUM(BlueprintType)
enum class EAzr_LocomotionHand : uint8 {
    LeftHand   UMETA(DisplayName = "Left Hand (Southpaw)"),
    RightHand  UMETA(DisplayName = "Right Hand (Standard)"),
    BothHands  UMETA(DisplayName = "Both Hands")
};

UENUM(BlueprintType)
enum class EForwardBehavior : uint8 {
    Teleport UMETA(DisplayName = "Teleport"),
    SmoothMove UMETA(DisplayName = "Smooth Movement")
};

UENUM(BlueprintType)
enum class ETurnBehavior : uint8 {
    SnapTurn UMETA(DisplayName = "Snap Turn"),
    SmoothTurn UMETA(DisplayName = "Smooth Turn")
};

UENUM(BlueprintType)
enum class EBackBehavior : uint8 {
    BlinkStep UMETA(DisplayName = "Blink Step (Fade Back)"),
    SmoothMove UMETA(DisplayName = "Smooth Move Back"),
    None UMETA(DisplayName = "Do Nothing")
};

UCLASS(ClassGroup = (AzurealXR))
class AZUREALXR_API AAzr_Pawn : public APawn {
    GENERATED_BODY()

public:
    AAzr_Pawn();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(
        class UInputComponent* PlayerInputComponent) override;

    // --- COMPONENTS ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    USceneComponent* VROrigin;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UCameraComponent* Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UWidgetComponent* GazeReticleWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UMotionControllerComponent* LeftMotionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UMotionControllerComponent* RightMotionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    USkeletalMeshComponent* LeftHandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    USkeletalMeshComponent* RightHandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UAzr_HandScanner* LeftHandScanner;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
    UAzr_HandScanner* RightHandScanner;

    // --- NEW: WIDGET INTERACTION (SMART LASERS) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UWidgetInteractionComponent* LeftWidgetInteraction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UWidgetInteractionComponent* RightWidgetInteraction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UStaticMeshComponent* LeftLaserBeam;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UStaticMeshComponent* RightLaserBeam;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UStaticMeshComponent* LeftLaserReticle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|UI Interaction")
    UStaticMeshComponent* RightLaserReticle;

    // --- UI INTERACTION CONFIG ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|UI Config")
    float MaxLaserDistance = 1500.0f;

    // Set this to the exact Trace Channel that your Azr_Collision profile blocks!
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|UI Config")
    TEnumAsByte<ECollisionChannel> LaserTraceChannel = ECC_Visibility;

    // --- INPUT ACTIONS ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Move;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Look;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Grab_Left;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Grab_Right;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Trigger_Left;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Azureal|Input")
    UInputAction* IA_Trigger_Right;

    // --- CONFIGURATION ---
    // NEW: Master Control for Ambidextrous Support
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    EAzr_LocomotionHand DominantLocomotionHand = EAzr_LocomotionHand::RightHand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    EForwardBehavior ForwardInput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    ETurnBehavior TurnInput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    EBackBehavior BackwardInput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    float SnapTurnAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    float SmoothTurnSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    float SmoothMoveSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Locomotion Config")
    float BlinkStepDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Animation")
    float HandAnimSpeed;

    // --- GAZE CONFIGURATION ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Gaze Config")
    float GazeTraceDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Gaze Config")
    float GazeTraceRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Gaze Config")
    bool bShowGazeDebug = false;

    UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
    void SetLeftHandMesh(USkeletalMesh* NewMesh);

    UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
    void SetRightHandMesh(USkeletalMesh* NewMesh);

    // --- ROOM SCALE LOCOMOTION ---
    UFUNCTION(BlueprintCallable, Category = "Azureal|Locomotion")
    void ExecuteRoomScaleTeleport(FVector TargetFloorLocation);

    // --- EVENT TELEPORT ---
    UFUNCTION(BlueprintCallable, Category = "Azureal|Locomotion")
    void TeleportPlayer(FVector TargetLocation, FRotator TargetRotation);

    // --- Enable/Disable LOCOMOTION STATE ---
    UFUNCTION(BlueprintCallable, Category = "Azureal|Locomotion")
    void EnableLocomotion();

    UFUNCTION(BlueprintCallable, Category = "Azureal|Locomotion")
    void DisableLocomotion();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|State")
    bool bIsVRMode;

    // --- THE MASTER LOCK ---
    bool bIsLocomotionEnabled = true;

    // --- NEW: TIMER HANDLE ---
    FTimerHandle EventTeleportTimerHandle;

    // --- STATE TRACKERS ---
    bool bReadyToSnapTurn;
    float TargetGripLeft;
    float CurrentGripLeft;
    float TargetGripRight;
    float CurrentGripRight;

    UPROPERTY()
    UAzr_Teleport* CachedTeleportComp;

    UPROPERTY()
    UAzr_HandAnimInstance* LeftHandAnim;

    UPROPERTY()
    UAzr_HandAnimInstance* RightHandAnim;

    void DetectPlatform();

    // NEW: Unified Ambidextrous Logic
    void ProcessStickInput(FVector2D AxisInput, UMotionControllerComponent* HandController, bool bIsLeftHand);

    void Move(const FInputActionValue& Value);
    void RightStickInput(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void GrabLeft(const FInputActionValue& Value);
    void ReleaseLeft(const FInputActionValue& Value);
    void GrabRight(const FInputActionValue& Value);
    void ReleaseRight(const FInputActionValue& Value);

    void OnTriggerLeft(const FInputActionValue& Value);
    void OnTriggerRight(const FInputActionValue& Value);

    void PassTriggerToHeldObject(UMotionControllerComponent* HandController, float Value);

    void ProcessGazeTrace();

    // Runs every frame to check UI hover states and stretch the visual beams
    void UpdateSmartLasers();

    // Track previous hover states to fire one-shot haptics
    bool bLeftWasHoveringWidget;
    bool bRightWasHoveringWidget;

    UPROPERTY()
    UAzr_ExplainWidget* LastHoveredLeftWidget;

    UPROPERTY()
    UAzr_ExplainWidget* LastHoveredRightWidget;
};