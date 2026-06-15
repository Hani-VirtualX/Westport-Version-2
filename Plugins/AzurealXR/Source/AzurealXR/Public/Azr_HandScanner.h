

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Azr_Grab.h"
#include "Azr_Latch.h"
#include "Azr_HandScanner.generated.h"

// Forward Declaration for our new Smart Hub
class UAzr_LatchZone;

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_HandScanner : public USceneComponent
{
	GENERATED_BODY()

public:
	UAzr_HandScanner();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- CONFIGURATION ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	FName InteractProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Config")
	bool bIsRightHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Debug")
	bool bShowDebugVisuals;

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Azureal|Components")
	UCapsuleComponent* InteractCapsule;

	// --- INPUT API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Input")
	void ProcessGrabInput(bool bIsPressed);

	// --- GETTERS ---
	// Allows the Pawn to pass trigger values directly to the held component
	FORCEINLINE UAzr_Latch* GetCurrentHeldLatch() const { return CurrentHeldLatch.Get(); }
	FORCEINLINE UAzr_Grab* GetCurrentHeldComponent() const { return CurrentHeldComponent.Get(); }

private:

	bool bIsGripPressed = false;

	// --- GRAB TRACKING (Physics Objects) ---
	TArray<TWeakObjectPtr<UAzr_Grab>> OverlappedGrabbables;

	UPROPERTY()
	TWeakObjectPtr<UAzr_Grab> CurrentHoveredComponent;

	UPROPERTY()
	TWeakObjectPtr<UAzr_Grab> CurrentHeldComponent;

	// --- LATCH TRACKING (Mechanical Parts) ---
	TArray<TWeakObjectPtr<UAzr_Latch>> OverlappedLatches;

	UPROPERTY()
	TWeakObjectPtr<UAzr_Latch> CurrentHoveredLatch;

	UPROPERTY()
	TWeakObjectPtr<UAzr_Latch> CurrentHeldLatch;

	// --- INTERNAL LOGIC ---
	void UpdateBestCandidate();

	UFUNCTION()
	void OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCapsuleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};