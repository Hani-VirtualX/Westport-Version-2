

#include "Azr_Grab.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h" 
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h" 
#include "Azr_Interactable.h" 
#include "Azr_Pointer.h" 
#include "Azr_Indicator.h"
#include "Azr_AttachTarget.h"
#include "Azr_HandScanner.h"
#include "Azr_ExplainWidget.h"
#include "Azr_ActionWidget.h" 
#include "Azr_LabelWidget.h" 

static int32 GlobalHighlightCount = 0;

UAzr_Grab::UAzr_Grab()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	StencilID = 252;
	HighlightSpeed = 1.2f;
	SnapSpeed = 10.0f;
	SnapThreshold = 3.0f;
	LastValue = 0.0f;
	bWasRising = false;

	bIsAttachMode = false;
	ActiveAttachID = 0;
	ClosestAttachTarget = nullptr;
	CurrentAttachedSocket = nullptr;
	CachedCameraManager = nullptr;

	bIsGrabRemoveMode = false;
	bHasTriggeredRemoval = false;
	bIsGrabTriggerMode = false;
	bIsTriggerPressed = false;

	// --- CONFIG DEFAULTS ---
	Grab.TargetMeshName = FName("TargetMesh");
	Grab.TetherSettings.TargetWidgetName = FName("TargetWidget");
	Grab.TetherSettings.AnchorScale = 0.03f;
	Grab.TetherSettings.CableWidth = 35.0f;

	GrabAttach.TargetMeshName = FName("TargetMesh");
	GrabAttach.TetherSettings.TargetWidgetName = FName("TargetWidget");
	GrabAttach.SequenceID = 1;

	GrabRemove.TargetMeshName = FName("TargetMesh");
	GrabRemove.RemovalDistance = 15.0f;

	GrabTrigger.TargetMeshName = FName("TargetMesh");
	GrabTrigger.TriggerHand = EAzr_HandType::Both;
	GrabTrigger.TriggerThreshold = 0.5f;

	// --- COMPONENT SETUP ---
	StartAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TetherStartAnchor"));
	StartAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartAnchor->SetCastShadow(false);
	StartAnchor->SetVisibility(false);

	EndAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TetherEndAnchor"));
	EndAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndAnchor->SetCastShadow(false);
	EndAnchor->SetVisibility(false);

	TetherCable = CreateDefaultSubobject<UCableComponent>(TEXT("TetherCable"));
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetVisibility(false);
	TetherCable->NumSegments = 20;
	TetherCable->SolverIterations = 4;
	TetherCable->CableLength = 0.0f;

	// --- RESTORED: HARDCODED TETHER ASSETS ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded())
	{
		Grab.TetherSettings.AnchorMesh = SphereMesh.Object;
		GrabAttach.TetherSettings.AnchorMesh = SphereMesh.Object;
		GrabRemove.TetherSettings.AnchorMesh = SphereMesh.Object;
		GrabTrigger.TetherSettings.AnchorMesh = SphereMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded())
	{
		Grab.TetherSettings.CableMaterial = CableMatAsset.Object;
		GrabAttach.TetherSettings.CableMaterial = CableMatAsset.Object;
		GrabRemove.TetherSettings.CableMaterial = CableMatAsset.Object;
		GrabTrigger.TetherSettings.CableMaterial = CableMatAsset.Object;
	}

	// --- ASSETS: HIGHLIGHT ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;

	// --- AUDIO: GRAB ASSETS ---
	static ConstructorHelpers::FObjectFinder<USoundBase> GrabSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Grab"));
	if (GrabSoundAsset.Succeeded())
	{
		SoundOnGrab = GrabSoundAsset.Object;
		SoundOnAttach = GrabSoundAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> ReleaseSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Release"));
	if (ReleaseSoundAsset.Succeeded())
	{
		SoundOnRelease = ReleaseSoundAsset.Object;
		SoundOnRemove = ReleaseSoundAsset.Object;
	}

	// --- HAPTICS: HARDCODED ASSETS ---
	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> HapticGrabAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/Haptic_Grab"));
	if (HapticGrabAsset.Succeeded())
	{
		HapticOnGrab = HapticGrabAsset.Object;
		HapticOnRelease = HapticGrabAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> HapticAttachAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/Haptic_GrabAttach"));
	if (HapticAttachAsset.Succeeded()) HapticOnAttach = HapticAttachAsset.Object;

	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> HapticRemoveAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/Haptic_GrabRemove"));
	if (HapticRemoveAsset.Succeeded()) HapticOnRemove = HapticRemoveAsset.Object;

	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> HapticTriggerAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/Haptic_GrabTrigger"));
	if (HapticTriggerAsset.Succeeded()) HapticOnTrigger = HapticTriggerAsset.Object;
}

void UAzr_Grab::BeginPlay()
{
	Super::BeginPlay();
	CachedCameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	EnsureInitialized();
}

void UAzr_Grab::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsGrabEnabled) DisableGrab();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TriggerResetTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void UAzr_Grab::EnsureInitialized()
{
	if (!GetOwner()) return;

	if (StartAnchor->GetAttachParent() == nullptr)
	{
		StartAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	if (!AutoDetectedMesh)
	{
		AutoDetectedMesh = FindMeshByName(FName("Target Mesh"));
		if (!AutoDetectedMesh)
		{
			TArray<UActorComponent*> MeshComps;
			GetOwner()->GetComponents(UStaticMeshComponent::StaticClass(), MeshComps);
			for (UActorComponent* Comp : MeshComps)
			{
				UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
				if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
				{
					AutoDetectedMesh = Mesh; break;
				}
			}
		}
	}

	if (GlobalHighlightCount == 0 && HighlightMPC && GetWorld() && !bIsGrabEnabled)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), 0.0f);
	}
}

// --- ENABLE API ---

void UAzr_Grab::EnableGrab()
{
	EnsureInitialized();
	if (bIsGrabEnabled) return;
	bIsGrabEnabled = true;

	SetComponentTickEnabled(true);
	bIsAttachMode = false;
	bIsGrabRemoveMode = false;
	bIsGrabTriggerMode = false;
	ClosestAttachTarget = nullptr;

	ApplyConfig(Grab);

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(false);
	}

	if (Grab.HighlightMode != EAzr_HighlightMode::None)
	{
		ToggleHighlight(true, Grab.HighlightMode);
		GlobalHighlightCount++;
		if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }

		AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);
	}

	UpdatePointer(false);

	CurrentTargetWidget = FindWidgetByName(Grab.TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		// --- INJECT DYNAMIC TEXT FOR ANY WIDGET TYPE ---
		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			// 1. Is it an Explain Widget?
			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(Grab.Description);
			}
			// 2. Is it an Action Widget?
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				// FIXED: Using your exact header function name
				ActionUI->SetActionDescription(Grab.Description);
			}
			// 3. Is it a Label Widget?
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				// FIXED: Using your exact header function name
				LabelUI->SetLabelText(Grab.Description);
			}
		}
	}

	ToggleTether(true, Grab.TetherSettings);
}

void UAzr_Grab::EnableGrabAttach(int32 SequenceID)
{
	EnsureInitialized();
	if (bIsGrabEnabled) return;
	bIsGrabEnabled = true;

	SetComponentTickEnabled(true);
	bIsAttachMode = true;
	bIsGrabRemoveMode = false;
	bIsGrabTriggerMode = false;
	ActiveAttachID = (SequenceID == 0) ? GrabAttach.SequenceID : SequenceID;

	ApplyConfig(GrabAttach);
	FindAndShowGhost();

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(false);
	}

	if (GrabAttach.HighlightMode != EAzr_HighlightMode::None)
	{
		ToggleHighlight(true, GrabAttach.HighlightMode);
		GlobalHighlightCount++;
		if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }

		AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);
	}

	UpdatePointer(false);

	CurrentTargetWidget = FindWidgetByName(GrabAttach.TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(GrabAttach.Description);
			}
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(GrabAttach.Description);
			}
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(GrabAttach.Description);
			}
		}
	}

	ToggleTether(true, GrabAttach.TetherSettings);
}

void UAzr_Grab::EnableGrabRemove()
{
	EnsureInitialized();
	if (bIsGrabEnabled) return;
	bIsGrabEnabled = true;

	SetComponentTickEnabled(true);
	bIsAttachMode = false;
	bIsGrabRemoveMode = true;
	bIsGrabTriggerMode = false;
	bHasTriggeredRemoval = false;
	ClosestAttachTarget = nullptr;

	GrabRemoveStartLocation = GetOwner()->GetActorLocation();

	ApplyConfig(GrabRemove);

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(false);
	}

	if (GrabRemove.HighlightMode != EAzr_HighlightMode::None)
	{
		ToggleHighlight(true, GrabRemove.HighlightMode);
		GlobalHighlightCount++;
		if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }

		AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);
	}

	UpdatePointer(false);

	CurrentTargetWidget = FindWidgetByName(GrabRemove.TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(GrabRemove.Description);
			}
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(GrabRemove.Description);
			}
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(GrabRemove.Description);
			}
		}
	}

	ToggleTether(true, GrabRemove.TetherSettings);
}

void UAzr_Grab::EnableGrabTrigger()
{
	EnsureInitialized();
	if (bIsGrabEnabled) return;
	bIsGrabEnabled = true;

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TriggerResetTimerHandle);

	SetComponentTickEnabled(true);
	bIsAttachMode = false;
	bIsGrabRemoveMode = false;
	bIsGrabTriggerMode = true;
	bIsTriggerPressed = false;
	ClosestAttachTarget = nullptr;

	ApplyConfig(GrabTrigger);

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(false);
	}

	if (GrabTrigger.HighlightMode != EAzr_HighlightMode::None)
	{
		ToggleHighlight(true, GrabTrigger.HighlightMode);
		GlobalHighlightCount++;
		if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }

		AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);
	}

	UpdatePointer(false);

	CurrentTargetWidget = FindWidgetByName(GrabTrigger.TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(GrabTrigger.Description);
			}
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(GrabTrigger.Description);
			}
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(GrabTrigger.Description);
			}
		}
	}

	ToggleTether(true, GrabTrigger.TetherSettings);
}

void UAzr_Grab::DisableGrab()
{
	if (!bIsGrabEnabled) return;
	bIsGrabEnabled = false;
	bHasTetherSettled = false;

	SetComponentTickEnabled(false);

	if (ClosestAttachTarget.IsValid())
	{
		ClosestAttachTarget->SetTetherAndWidgetVisibility(false);
		ClosestAttachTarget->SetGhostVisibility(false);
		ClosestAttachTarget = nullptr;
	}

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->GrabZone) Parent->GrabZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(true);
	}

	ToggleHighlight(false, EAzr_HighlightMode::AllComponents);

	GlobalHighlightCount--;
	if (GlobalHighlightCount < 0) GlobalHighlightCount = 0;

	if (GlobalHighlightCount == 0 && HighlightMPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), 0.0f);
	}

	if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();

	ToggleTether(false, Grab.TetherSettings);
	ToggleTether(false, GrabAttach.TetherSettings);
	ToggleTether(false, GrabRemove.TetherSettings);
	ToggleTether(false, GrabTrigger.TetherSettings);

	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(false);
		CurrentTargetWidget = nullptr;
	}
}

void UAzr_Grab::DisableGrabAttach()
{
	DisableGrab();
}

void UAzr_Grab::DisableGrabRemove(bool bResetTransform)
{
	bIsGrabRemoveMode = false;
	DisableGrab();
	ForceRelease();

	if (bResetTransform)
	{
		GetOwner()->SetActorTransform(GrabRemove.ResetTransform);
	}
}

void UAzr_Grab::DisableGrabTrigger(bool bResetTransform)
{
	bIsGrabTriggerMode = false;
	DisableGrab();
	ForceRelease();

	if (bResetTransform && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(TriggerResetTimerHandle, this, &UAzr_Grab::ExecuteTriggerReset, 1.0f, false);
	}
}

void UAzr_Grab::ExecuteTriggerReset()
{
	if (GetOwner())
	{
		GetOwner()->SetActorTransform(GrabTrigger.ResetTransform);
	}
}

// --- RUNTIME SETTERS ---

void UAzr_Grab::SetTriggerHand(EAzr_HandType NewHand)
{
	GrabTrigger.TriggerHand = NewHand;
}

void UAzr_Grab::SetTriggerThreshold(float NewThreshold)
{
	GrabTrigger.TriggerThreshold = FMath::Clamp(NewThreshold, 0.0f, 1.0f);
}

void UAzr_Grab::FindAndShowGhost()
{
	ClosestAttachTarget = nullptr;
	FVector MyLoc = GetOwner()->GetActorLocation();

	TArray<FHitResult> Hits;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(5000.0f);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	GetWorld()->SweepMultiByChannel(Hits, MyLoc, MyLoc, FQuat::Identity, ECC_WorldDynamic, Sphere, QueryParams);

	TArray<FHitResult> StaticHits;
	GetWorld()->SweepMultiByChannel(StaticHits, MyLoc, MyLoc, FQuat::Identity, ECC_WorldStatic, Sphere, QueryParams);
	Hits.Append(StaticHits);

	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.GetActor()) continue;
		TArray<UAzr_AttachTarget*> Targets;
		Hit.GetActor()->GetComponents(Targets);

		for (UAzr_AttachTarget* Target : Targets)
		{
			if (!Target || Target->bIsFilled) continue;
			if (!Target->CompatibleClass) continue;
			if (!GetOwner()->GetClass()->IsChildOf(Target->CompatibleClass)) continue;
			if (ActiveAttachID > 0 && Target->AttachSequenceID != ActiveAttachID) continue;

			Target->SetGhostVisibility(true);
			ClosestAttachTarget = Target;
			return;
		}
	}
}

void UAzr_Grab::UpdatePointer(bool bIsGrabbing)
{
	UAzr_Pointer* Pointer = FindPlayerPointer();
	if (!Pointer) return;

	if (bIsAttachMode && bIsGrabbing && ClosestAttachTarget.IsValid())
	{
		Pointer->EnablePointer_TargetComponent(ClosestAttachTarget.Get());
	}
	else
	{
		USceneComponent* Target = CurrentTargetMesh ? Cast<USceneComponent>(CurrentTargetMesh) : GetOwner()->GetRootComponent();
		Pointer->EnablePointer_TargetComponent(Target);
	}
}

// --- GRAB & RELEASE API ---

void UAzr_Grab::SnapActorToHand(USceneComponent* Hand, USceneComponent* SnapPoint)
{
	if (!bIsGrabEnabled || !Hand) return;

	if (CurrentAttachedSocket.IsValid())
	{
		if (OnGrabDetached.IsBound())
		{
			int32 OldID = CurrentAttachedSocket->AttachSequenceID;
			OnGrabDetached.Broadcast(OldID, CurrentAttachedSocket.Get());
		}
		CurrentAttachedSocket->NotifyObjectDetached();
		CurrentAttachedSocket = nullptr;
	}

	GetOwner()->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	CurrentHand = Hand;
	bIsSnapping = true;
	bIsAttached = false;

	// --- AUDIO: ON GRAB ---
	if (SoundOnGrab)
	{
		UGameplayStatics::SpawnSoundAttached(SoundOnGrab, GetOwner()->GetRootComponent());
	}

	// --- HAPTICS: ON GRAB ---
	if (HapticOnGrab)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			bool bIsRight = Hand->ComponentHasTag(FName("Right"));
			EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
			PC->PlayHapticEffect(HapticOnGrab, TargetHand, 1.0f, false);
		}
	}

	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		if (RootPrim->IsSimulatingPhysics()) RootPrim->SetSimulatePhysics(false);
	}

	ToggleHighlight(false, EAzr_HighlightMode::AllComponents);

	if (bIsAttachMode) ToggleTether(false, GrabAttach.TetherSettings);
	else if (bIsGrabRemoveMode) ToggleTether(false, GrabRemove.TetherSettings);
	else if (bIsGrabTriggerMode) ToggleTether(false, GrabTrigger.TetherSettings);
	else ToggleTether(false, Grab.TetherSettings);

	if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(false);

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(true);
	}

	if (bIsAttachMode)
	{
		if (!ClosestAttachTarget.IsValid()) FindAndShowGhost();
		if (ClosestAttachTarget.IsValid())
		{
			ClosestAttachTarget->SetTetherAndWidgetVisibility(true);
			UpdatePointer(true);
		}
	}
	else
	{
		if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();
	}

	// --- THE WORLD SPACE FIX ---
	// Instead of relying on rigid local hierarchy math, we dynamically calculate 
	// the absolute offset between the main Actor and the current SnapPoint location.
	if (SnapPoint)
	{
		TargetRelativeTransform = GetOwner()->GetActorTransform().GetRelativeTransform(SnapPoint->GetComponentTransform());
	}
	else
	{
		TargetRelativeTransform = FTransform::Identity;
	}

	if (OnGrabbed.IsBound()) OnGrabbed.Broadcast();
}

void UAzr_Grab::ReleaseHand()
{
	if (!CurrentHand) return;

	// --- AUDIO: ON RELEASE ---
	if (SoundOnRelease)
	{
		UGameplayStatics::SpawnSoundAttached(SoundOnRelease, GetOwner()->GetRootComponent());
	}

	// --- HAPTICS: ON RELEASE ---
	if (HapticOnRelease)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			bool bIsRight = CurrentHand->ComponentHasTag(FName("Right"));
			EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
			PC->PlayHapticEffect(HapticOnRelease, TargetHand, 1.0f, false);
		}
	}

	GetOwner()->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bIsSnapping = false;
	bIsAttached = false;
	CurrentHand = nullptr;

	if (bIsGrabEnabled)
	{
		const FAzr_GrabConfig* CurrentConfig = &Grab;
		if (bIsAttachMode) CurrentConfig = (FAzr_GrabConfig*)&GrabAttach;
		else if (bIsGrabRemoveMode) CurrentConfig = (FAzr_GrabConfig*)&GrabRemove;
		else if (bIsGrabTriggerMode) CurrentConfig = (FAzr_GrabConfig*)&GrabTrigger;

		if (CurrentConfig->HighlightMode != EAzr_HighlightMode::None)
		{
			ToggleHighlight(true, CurrentConfig->HighlightMode);
			if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }
		}

		ToggleTether(true, CurrentConfig->TetherSettings);
		if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(true);

		if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
		{
			if (Parent->SpawnedIndicator) Parent->SpawnedIndicator->SetActorHiddenInGame(false);
		}

		if (bIsAttachMode)
		{
			if (ClosestAttachTarget.IsValid()) ClosestAttachTarget->SetTetherAndWidgetVisibility(false);
			UpdatePointer(false);
		}
		else
		{
			if (UAzr_Pointer* Pointer = FindPlayerPointer())
			{
				USceneComponent* Target = CurrentTargetMesh ? Cast<USceneComponent>(CurrentTargetMesh) : GetOwner()->GetRootComponent();
				Pointer->EnablePointer_TargetComponent(Target);
			}
		}
	}

	if (OnReleased.IsBound()) OnReleased.Broadcast();
}

void UAzr_Grab::ForceRelease()
{
	ReleaseHand();
}

void UAzr_Grab::NotifyAttached(int32 ID, UAzr_AttachTarget* Target)
{
	CurrentAttachedSocket = Target;

	// --- AUDIO: ON ATTACH ---
	if (SoundOnAttach)
	{
		UGameplayStatics::SpawnSoundAttached(SoundOnAttach, GetOwner()->GetRootComponent());
	}

	// --- HAPTICS: ON ATTACH ---
	if (CurrentHand && HapticOnAttach)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			bool bIsRight = CurrentHand->ComponentHasTag(FName("Right"));
			EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
			PC->PlayHapticEffect(HapticOnAttach, TargetHand, 1.0f, false);
		}
	}

	if (OnGrabAttached.IsBound())
	{
		if (IsValid(Target))
		{
			OnGrabAttached.Broadcast(ID, Target);
		}
	}
}

void UAzr_Grab::HandleTriggerInput(float Value)
{
	if (!bIsGrabTriggerMode || !CurrentHand) return;

	if (!IsHandCompatible(CurrentHand)) return;

	if (Value >= GrabTrigger.TriggerThreshold)
	{
		if (!bIsTriggerPressed)
		{
			bIsTriggerPressed = true;

			// --- HAPTICS: ON TRIGGER ---
			if (HapticOnTrigger)
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
				{
					bool bIsRight = CurrentHand->ComponentHasTag(FName("Right"));
					EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
					PC->PlayHapticEffect(HapticOnTrigger, TargetHand, 1.0f, false);
				}
			}

			if (OnGrabTriggered.IsBound())
			{
				OnGrabTriggered.Broadcast(Value);
			}
		}
	}
	else
	{
		if (bIsTriggerPressed)
		{
			bIsTriggerPressed = false;
			if (OnGrabTriggerReleased.IsBound())
			{
				OnGrabTriggerReleased.Broadcast(Value);
			}
		}
	}
}

void UAzr_Grab::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// --- GRAB REMOVE DISTANCE CHECK ---
	if (bIsGrabRemoveMode && CurrentHand && !bHasTriggeredRemoval)
	{
		float Dist = FVector::Dist(GetOwner()->GetActorLocation(), GrabRemoveStartLocation);
		if (Dist >= GrabRemove.RemovalDistance)
		{
			bHasTriggeredRemoval = true;

			// --- AUDIO: ON REMOVE ---
			if (SoundOnRemove)
			{
				UGameplayStatics::SpawnSoundAttached(SoundOnRemove, GetOwner()->GetRootComponent());
			}

			// --- HAPTICS: ON REMOVE ---
			if (HapticOnRemove)
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
				{
					bool bIsRight = CurrentHand->ComponentHasTag(FName("Right"));
					EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
					PC->PlayHapticEffect(HapticOnRemove, TargetHand, 1.0f, false);
				}
			}

			if (OnGrabRemoved.IsBound())
			{
				OnGrabRemoved.Broadcast();
			}
		}
	}

	if (bIsGrabEnabled && CurrentTargetWidget)
	{
		FVector TargetLoc = FVector::ZeroVector;
		if (CachedCameraManager) TargetLoc = CachedCameraManager->GetCameraLocation();
		else
		{
			if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
			{
				TargetLoc = CamManager->GetCameraLocation();
				CachedCameraManager = CamManager;
			}
		}

		if (!TargetLoc.IsZero())
		{
			FVector StartLoc = CurrentTargetWidget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc);
			CurrentTargetWidget->SetWorldRotation(NewRot);
		}

		// --- FIX: Dynamically update the tether anchor every frame! ---
		if (EndAnchor)
		{
			const FAzr_TetherConfig* ActiveTether = &Grab.TetherSettings;
			if (bIsAttachMode) ActiveTether = &GrabAttach.TetherSettings;
			else if (bIsGrabRemoveMode) ActiveTether = &GrabRemove.TetherSettings;
			else if (bIsGrabTriggerMode) ActiveTether = &GrabTrigger.TetherSettings;

			if (ActiveTether->bEnableTether)
			{
				FVector DynamicEndPos = CalculateSurfaceAnchor(CurrentTargetWidget, ActiveTether->WidgetAnchorPos, *ActiveTether);
				EndAnchor->SetWorldLocation(DynamicEndPos);

				// --- RIGID OVERRIDE ---
				float ActualDistance = FVector::Dist(StartAnchor->GetComponentLocation(), DynamicEndPos);
				float Slack = ActiveTether->CableHang;

				if (Slack <= 0.1f)
				{
					// Force the cable to the exact distance with no gravity
					TetherCable->CableLength = ActualDistance;
					TetherCable->CableGravityScale = 0.0f;
				}
				else
				{
					TetherCable->CableLength = ActualDistance + Slack;
					TetherCable->CableGravityScale = 1.0f;
				}

				// Flush physics momentum to kill wobble
				TetherCable->SetRelativeLocation(FVector::ZeroVector);
				TetherCable->EndLocation = FVector::ZeroVector;
			}
		}
	}

	if (bIsGrabEnabled) UpdatePointer(CurrentHand != nullptr);

	// --- SNAPPING LOGIC ---
	if (bIsSnapping && CurrentHand && !bIsAttached)
	{
		FTransform HandWorld = CurrentHand->GetComponentTransform();
		FTransform GoalTransform = TargetRelativeTransform * HandWorld;
		FVector NewLoc = FMath::VInterpTo(GetOwner()->GetActorLocation(), GoalTransform.GetLocation(), DeltaTime, SnapSpeed);
		FQuat NewRot = FMath::QInterpTo(GetOwner()->GetActorQuat(), GoalTransform.GetRotation(), DeltaTime, SnapSpeed);
		GetOwner()->SetActorLocationAndRotation(NewLoc, NewRot);
		if (FVector::DistSquared(NewLoc, GoalTransform.GetLocation()) < (SnapThreshold * SnapThreshold))
		{
			bIsAttached = true;
			bIsSnapping = false;
			GetOwner()->AttachToComponent(CurrentHand, FAttachmentTransformRules::KeepWorldTransform);
			GetOwner()->SetActorRelativeTransform(TargetRelativeTransform);
		}
	}

	// --- IDLE HIGHLIGHT PULSE ---
	if (bIsGrabEnabled && !CurrentHand && HighlightMPC)
	{
		float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
		float WorldTime = GetWorld()->GetTimeSeconds();
		float Phase = (WorldTime * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
		float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), CurrentValue);

		bool bIsRising = (CurrentValue > LastValue);
		if (CurrentValue < 0.05f && bIsRising && !bWasRising && SoundHighlightStart)
			UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
		if (bWasRising && !bIsRising && SoundHighlightEnd)
			UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());

		LastValue = CurrentValue;
		bWasRising = bIsRising;
	}
}

void UAzr_Grab::ApplyConfig(const FAzr_GrabConfig& InputConfig)
{
	if (!AutoDetectedMesh) EnsureInitialized();

	if (InputConfig.TargetMeshName.IsNone()) CurrentTargetMesh = AutoDetectedMesh;
	else
	{
		CurrentTargetMesh = FindMeshByName(InputConfig.TargetMeshName);
		if (!CurrentTargetMesh) CurrentTargetMesh = AutoDetectedMesh;
	}

	if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
	{
		if (Parent->GrabZone)
		{
			// --- THE FIX: The Physics Profile Race Condition ---

			// 1. Set the profile name FIRST so the HandScanner reads the right ID
			Parent->GrabZone->SetCollisionProfileName(FName("Azr_Collision"));

			// 2. Enable the collision
			Parent->GrabZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

			// 3. The "Physics Flush" - Toggle overlaps to force the engine to hard-rebuild the state
			Parent->GrabZone->SetGenerateOverlapEvents(false);
			Parent->GrabZone->SetGenerateOverlapEvents(true);

			// 4. Perform the manual sweep
			Parent->GrabZone->UpdateOverlaps();
		}
	}
}

void UAzr_Grab::ToggleHighlight(bool bState, EAzr_HighlightMode Mode)
{
	if (Mode == EAzr_HighlightMode::None) return;
	TArray<UMeshComponent*> MeshesToHighlight;

	if (Mode == EAzr_HighlightMode::AllComponents) GetOwner()->GetComponents(MeshesToHighlight);
	else if (Mode == EAzr_HighlightMode::TargetMeshOnly)
	{
		if (UMeshComponent* Target = Cast<UMeshComponent>(CurrentTargetMesh)) MeshesToHighlight.Add(Target);
	}

	// --- THE EXECUTION ORDER FIX ---
	// If the widget hasn't been loaded into memory yet, force the engine to find it right now!
	if (!CurrentTargetWidget)
	{
		const FAzr_GrabConfig* CurrentConfig = &Grab;
		if (bIsAttachMode) CurrentConfig = (const FAzr_GrabConfig*)&GrabAttach;
		else if (bIsGrabRemoveMode) CurrentConfig = (const FAzr_GrabConfig*)&GrabRemove;
		else if (bIsGrabTriggerMode) CurrentConfig = (const FAzr_GrabConfig*)&GrabTrigger;

		CurrentTargetWidget = FindWidgetByName(CurrentConfig->TetherSettings.TargetWidgetName);
	}

	// Now that we guarantee it exists, make it glow
	if (UMeshComponent* WidgetMesh = Cast<UMeshComponent>(CurrentTargetWidget)) {
		MeshesToHighlight.AddUnique(WidgetMesh);
	}

	for (UMeshComponent* Mesh : MeshesToHighlight)
	{
		if (Mesh)
		{
			Mesh->SetRenderCustomDepth(bState);
			Mesh->SetCustomDepthStencilValue(StencilID);
		}
	}
}

void UAzr_Grab::ToggleTether(bool bState, const FAzr_TetherConfig& TetherConfig)
{
	if (!bState || !TetherConfig.bEnableTether)
	{
		StartAnchor->SetVisibility(false); EndAnchor->SetVisibility(false); TetherCable->SetVisibility(false); return;
	}

	USceneComponent* MeshTarget = CurrentTargetMesh;
	USceneComponent* WidgetTarget = FindWidgetByName(TetherConfig.TargetWidgetName);
	if (!MeshTarget || !WidgetTarget) return;

	if (TetherConfig.AnchorMesh)
	{
		StartAnchor->SetStaticMesh(TetherConfig.AnchorMesh);
		EndAnchor->SetStaticMesh(TetherConfig.AnchorMesh);
	}
	StartAnchor->SetWorldScale3D(FVector(TetherConfig.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherConfig.AnchorScale));

	if (TetherConfig.CableMaterial) TetherCable->SetMaterial(0, TetherConfig.CableMaterial);
	TetherCable->CableWidth = TetherConfig.CableWidth;

	// --- DRIFT FIX: Lazy Attachment Check ---
	bool bStartCorrect = (StartAnchor->GetAttachParent() == MeshTarget);
	bool bEndCorrect = (EndAnchor->GetAttachParent() == WidgetTarget);

	if (!bStartCorrect || !bEndCorrect)
	{
		FVector StartPos = CalculateSurfaceAnchor(MeshTarget, TetherConfig.MeshAnchorPos, TetherConfig);
		FVector EndPos = CalculateSurfaceAnchor(WidgetTarget, TetherConfig.WidgetAnchorPos, TetherConfig);

		if (!bStartCorrect) StartAnchor->AttachToComponent(MeshTarget, FAttachmentTransformRules::KeepWorldTransform);
		if (!bEndCorrect) EndAnchor->AttachToComponent(WidgetTarget, FAttachmentTransformRules::KeepWorldTransform);

		StartAnchor->SetWorldLocation(StartPos);
		EndAnchor->SetWorldLocation(EndPos);
	}

	TetherCable->SetAttachEndToComponent(EndAnchor);
	TetherCable->SetRelativeLocation(FVector::ZeroVector);
	TetherCable->EndLocation = FVector::ZeroVector;

	// --- THE FIX: NUKE ALL COLLISION ---
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetCollisionResponseToAllChannels(ECR_Ignore);
	TetherCable->bEnableCollision = false;

	// --- PERCENTAGE MATH & STIFF HANG ---
	float HangPercentage = TetherConfig.CableHang / 100.0f;
	float InitialDist = FVector::Dist(StartAnchor->GetComponentLocation(), EndAnchor->GetComponentLocation());

	if (HangPercentage <= 0.001f) {
		// 0% Hang = 1 Segment Straight Line
		TetherCable->NumSegments = 1;
		TetherCable->CableLength = InitialDist;
		TetherCable->CableGravityScale = 0.0f;
		TetherCable->bEnableStiffness = false;
	}
	else {
		// > 0% Hang = 20 Segments
		TetherCable->NumSegments = 20;

		// Calculate slack based on the percentage of the distance!
		float SlackAmount = InitialDist * HangPercentage;
		TetherCable->CableLength = InitialDist + SlackAmount;

		// Gentle gravity scaling based on percentage
		TetherCable->CableGravityScale = FMath::Clamp(HangPercentage * 0.5f, 0.01f, 0.5f);

		// --- THE STIFF HANG FIX ---
		TetherCable->bEnableStiffness = true;
		TetherCable->SolverIterations = 16;
	}

	TetherCable->RecreatePhysicsState();

	StartAnchor->SetVisibility(true);
	EndAnchor->SetVisibility(true);

	// --- THE FIRST-TIME ONLY TIMER LOGIC ---
	if (!bHasTetherSettled)
	{
		// First time ever: Hide it and wait 0.2s for physics to settle
		bHasTetherSettled = true;
		TetherCable->SetVisibility(false);

		if (UWorld* World = GetWorld()) {
			FTimerHandle SettleTimer;
			World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateWeakLambda(this, [this]() {
				// Only show if the player hasn't already grabbed it during this 0.2s window
				if (bIsGrabEnabled && TetherCable) {
					TetherCable->SetVisibility(true);
				}
				}), 0.20f, false);
		}
	}
	else
	{
		// Every other time (Unhiding after dropping the object): Show instantly
		TetherCable->SetVisibility(true);
	}
}

bool UAzr_Grab::IsHandCompatible(USceneComponent* Hand) const
{
	if (!Hand) return false;
	if (GrabTrigger.TriggerHand == EAzr_HandType::Both) return true;

	// Optimized Tag Check
	bool bIsLeft = Hand->ComponentHasTag(FName("Left"));
	bool bIsRight = Hand->ComponentHasTag(FName("Right"));

	if (GrabTrigger.TriggerHand == EAzr_HandType::Left) return bIsLeft;
	if (GrabTrigger.TriggerHand == EAzr_HandType::Right) return bIsRight;

	return false;
}

FVector UAzr_Grab::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& TetherConfig)
{
	if (!Target) return FVector::ZeroVector;

	// --- WIDGET LOGIC ---
	if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(Target))
	{
		FVector2D DrawSize = WidgetComp->GetCurrentDrawSize();
		FVector Scale = WidgetComp->GetComponentScale();
		FVector2D Pivot = WidgetComp->GetPivot();

		float WorldHalfWidth = (DrawSize.X * 0.5f) * Scale.Y;
		float WorldHalfHeight = (DrawSize.Y * 0.5f) * Scale.Z;

		FVector Center = WidgetComp->GetComponentLocation();
		FVector RightVec = WidgetComp->GetRightVector();
		FVector UpVec = WidgetComp->GetUpVector();
		FVector ForwardVec = WidgetComp->GetForwardVector();

		float PivotShiftX = (0.5f - Pivot.X) * (DrawSize.X * Scale.Y);
		float PivotShiftY = (0.5f - Pivot.Y) * (DrawSize.Y * Scale.Z);
		FVector VisualCenter = Center + (RightVec * PivotShiftX) + (UpVec * PivotShiftY);

		switch (Pos)
		{
		case EAzr_TetherPos::Top: return VisualCenter + (UpVec * (WorldHalfHeight + TetherConfig.WidgetGap_Vertical));
		case EAzr_TetherPos::Bottom: return VisualCenter - (UpVec * (WorldHalfHeight + TetherConfig.WidgetGap_Vertical));
		case EAzr_TetherPos::Right: return VisualCenter + (RightVec * (WorldHalfWidth + TetherConfig.WidgetGap_Horizontal));
		case EAzr_TetherPos::Left: return VisualCenter - (RightVec * (WorldHalfWidth + TetherConfig.WidgetGap_Horizontal));
		case EAzr_TetherPos::Front: return VisualCenter + (ForwardVec * 1.0f);
		case EAzr_TetherPos::Back: return VisualCenter - (ForwardVec * 1.0f);
		default: return VisualCenter;
		}
	}

	// --- MESH LOGIC ---
	FBoxSphereBounds LocalBounds = Target->CalcLocalBounds();
	FVector WorldCenter = Target->GetComponentTransform().TransformPosition(LocalBounds.Origin);
	FVector ForwardVec = Target->GetForwardVector();
	FVector RightVec = Target->GetRightVector();
	FVector UpVec = Target->GetUpVector();
	FVector Scale = Target->GetComponentScale();

	float WorldHalfDepth = LocalBounds.BoxExtent.X * FMath::Abs(Scale.X);
	float WorldHalfWidth = LocalBounds.BoxExtent.Y * FMath::Abs(Scale.Y);
	float WorldHalfHeight = LocalBounds.BoxExtent.Z * FMath::Abs(Scale.Z);

	FVector SurfacePush = FVector::ZeroVector;

	switch (Pos)
	{
	case EAzr_TetherPos::Top: SurfacePush = UpVec * (WorldHalfHeight + TetherConfig.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Bottom: SurfacePush = -UpVec * (WorldHalfHeight + TetherConfig.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Right: SurfacePush = RightVec * (WorldHalfWidth + TetherConfig.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Left: SurfacePush = -RightVec * (WorldHalfWidth + TetherConfig.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Front: SurfacePush = ForwardVec * (WorldHalfDepth + TetherConfig.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Back: SurfacePush = -ForwardVec * (WorldHalfDepth + TetherConfig.MeshSurfaceOffset); break;
	default: break;
	}

	return WorldCenter + SurfacePush + (UpVec * TetherConfig.MeshOffset_Vertical) + (RightVec * TetherConfig.MeshOffset_Horizontal);
}

USceneComponent* UAzr_Grab::FindWidgetByName(FName Name)
{
	if (Name.IsNone()) return nullptr;
	TArray<USceneComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (USceneComponent* Comp : Comps)
	{
		if (Comp->IsA(UWidgetComponent::StaticClass()) && (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())))
			return Comp;
	}
	return nullptr;
}

UPrimitiveComponent* UAzr_Grab::FindMeshByName(FName Name)
{
	TArray<UPrimitiveComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (UPrimitiveComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString()))
			return Cast<UPrimitiveComponent>(Comp);
	}
	return nullptr;
}

UAzr_Pointer* UAzr_Grab::FindPlayerPointer() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	return nullptr;
}