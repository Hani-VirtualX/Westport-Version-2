



#include "Azr_Gaze.h"
#include "Azr_Interactable.h"
#include "Azr_Pointer.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"

#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Azr_Pawn.h" 
#include "Azr_GazeWidget.h"
#include "Azr_Indicator.h" 
#include "Engine/BlueprintGeneratedClass.h" 
#include "GameFramework/PlayerController.h" 

// --------------------

UAzr_Gaze::UAzr_Gaze()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// --- AZUREAL COLLISION STANDARD ---
	SetCollisionEnabled(ECollisionEnabled::NoCollision); // Safe by default
	SetCollisionProfileName(FName("NoCollision"));
	InitBoxExtent(FVector(32.0f, 32.0f, 32.0f));

	// --- DEFAULTS ---
	InteractID = 1;
	GazeDuration = 2.0f;
	DrainRate = 1.5f;
	bResetOnTrigger = true;
	HighlightSpeed = 1.2f;
	
	SpawnedIndicator = nullptr;

	StencilID = 252;
	LastHighlightValue = 0.0f;
	bWasRising = false;

	// Initialize Hover Memory
	bWasLookedAt = false;

	
	// --- ASSET INITIALIZATION ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	
	// --- AUDIO ASSETS ---
	static ConstructorHelpers::FObjectFinder<USoundBase> HighlightStartAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (HighlightStartAsset.Succeeded()) SoundHighlightStart = HighlightStartAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> HighlightEndAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (HighlightEndAsset.Succeeded()) SoundHighlightEnd = HighlightEndAsset.Object;

	// --- UPDATED GAZE SOUND PATHWAYS ---
	static ConstructorHelpers::FObjectFinder<USoundBase> GazeStartAsset(TEXT("/AzurealXR/Interaction/Gaze/OnGazeSound"));
	if (GazeStartAsset.Succeeded()) SoundGazeStart = GazeStartAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> GazeCompleteAsset(TEXT("/AzurealXR/Interaction/Gaze/SFX_GazeCompleted"));
	if (GazeCompleteAsset.Succeeded()) SoundGazeCompleted = GazeCompleteAsset.Object;
}

void UAzr_Gaze::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();

	// --- RUNTIME INDICATOR SPAWNING ---
	if (GazeIndicatorClass && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();

		SpawnedIndicator = GetWorld()->SpawnActor<AAzr_Indicator>(GazeIndicatorClass, GetComponentTransform(), SpawnParams);

		if (SpawnedIndicator)
		{
			SpawnedIndicator->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SpawnedIndicator->SetActorHiddenInGame(true); // Hidden until EnableGaze is called
		}
	}
}

void UAzr_Gaze::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsGazeEnabled) DisableGaze();
	Super::EndPlay(EndPlayReason);
}

// --- INTERFACE ---

void UAzr_Gaze::EnableGaze()
{
	if (bIsGazeEnabled) return;

	EnsureInitialized();

	bIsGazeEnabled = true;
	bHasTriggered = false;
	CurrentChargeTime = 0.0f;
	LastHighlightValue = 0.0f;
	bWasRising = true;
	bWasLookedAt = false; // Reset hover memory

	// 1. Wake up the Collision so the Pawn can see it
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionProfileName(FName("Azr_Collision"));

	

	if (SpawnedIndicator) SpawnedIndicator->SetActorHiddenInGame(false);

	// --- THE AUTO-UI BRIDGE ---
	if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (Pawn->GazeReticleWidget)
		{
			Pawn->GazeReticleWidget->SetVisibility(true);
			if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject()))
			{
				GazeUI->SetProgress(0.0f);
			}
		}
	}

	AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);

	// 3. Activate Visual Feedback & Shift Pointer
	ToggleHighlight(true);
	UpdatePointerZOffset(true);
	UpdatePointer();

	SetComponentTickEnabled(true);
}

void UAzr_Gaze::DisableGaze()
{
	if (!bIsGazeEnabled) return;

	bIsGazeEnabled = false;
	CurrentChargeTime = 0.0f;
	OnGazeProgressUpdated.Broadcast(0.0f);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
		PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
	}

	if (SpawnedIndicator)
	{
		if (bWasLookedAt) SpawnedIndicator->OnShrink();
		SpawnedIndicator->SetActorHiddenInGame(true);
	}

	if (bIsBeingLookedAt || bWasLookedAt)
	{
		TArray<UAzr_Gaze*> WakeZones;
		GetOwner()->GetComponents(WakeZones);
		for (UAzr_Gaze* Zone : WakeZones)
		{
			if (Zone != this && Zone->bIsGazeEnabled) Zone->ToggleHighlight(true);
		}
	}

	// --- THE SIBLING CHECK ---
	bool bOtherZonesActive = false;
	TArray<UAzr_Gaze*> AllZones;
	GetOwner()->GetComponents(AllZones);
	for (UAzr_Gaze* Zone : AllZones)
	{
		if (Zone != this && Zone->bIsGazeEnabled)
		{
			bOtherZonesActive = true;
			break;
		}
	}

	// Only turn off global systems if we are the LAST active zone!
	if (!bOtherZonesActive)
	{
		if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();

		if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (Pawn->GazeReticleWidget)
			{
				if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject())) GazeUI->SetProgress(0.0f);
				Pawn->GazeReticleWidget->SetVisibility(false);
			}
		}

		// FIXED: Only drop the pointer offset if NO OTHER zones need it!
		UpdatePointerZOffset(false);
	}

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(FName("NoCollision"));

	ToggleHighlight(false);

	SetComponentTickEnabled(false);
}

void UAzr_Gaze::SetIsBeingLookedAt()
{
	bIsBeingLookedAt = true;
}

// --- CORE LOGIC ---

void UAzr_Gaze::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsGazeEnabled) return;

	// --- 1. HOVER ENTER / EXIT (One-Shot Events) ---
	if (bIsBeingLookedAt && !bWasLookedAt)
	{
		if (SpawnedIndicator) SpawnedIndicator->OnExpand();
		if (SoundGazeStart) UGameplayStatics::SpawnSoundAttached(SoundGazeStart, this);

		// NEW: Turn OFF highlight for all other active zones
		TArray<UAzr_Gaze*> AllZones;
		GetOwner()->GetComponents(AllZones);
		for (UAzr_Gaze* Zone : AllZones)
		{
			if (Zone != this && Zone->bIsGazeEnabled) Zone->ToggleHighlight(false);
		}
	}
	else if (!bIsBeingLookedAt && bWasLookedAt)
	{
		if (SpawnedIndicator) SpawnedIndicator->OnShrink();

		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
			PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
		}

		// NEW: Turn ON highlight for all other active zones
		TArray<UAzr_Gaze*> AllZones;
		GetOwner()->GetComponents(AllZones);
		for (UAzr_Gaze* Zone : AllZones)
		{
			if (Zone != this && Zone->bIsGazeEnabled) Zone->ToggleHighlight(true);
		}
	}

	

	// --- 3. PROGRESS MATH & CONTINUOUS MAX HAPTICS ---
	if (!bHasTriggered)
	{
		float PreviousTime = CurrentChargeTime;

		if (bIsBeingLookedAt)
		{
			if (CurrentChargeTime == 0.0f)
			{
				UpdatePointer();
			}

			CurrentChargeTime += DeltaTime;

			// THE SUSTAIN: Keep sending 1.0 (Max Power) every frame we are looking
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Left);
				PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Right);
			}
		}
		else
		{
			if (CurrentChargeTime > 0.0f)
			{
				CurrentChargeTime -= (DeltaTime * DrainRate);
				if (CurrentChargeTime <= 0.0f)
				{
					CurrentChargeTime = 0.0f;
				
				}
			}
		}

		CurrentChargeTime = FMath::Clamp(CurrentChargeTime, 0.0f, GazeDuration);

		if (!FMath::IsNearlyEqual(PreviousTime, CurrentChargeTime, 0.001f))
		{
			float Progress = (GazeDuration > 0.0f) ? (CurrentChargeTime / GazeDuration) : 1.0f;
			OnGazeProgressUpdated.Broadcast(Progress);

			if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				if (Pawn->GazeReticleWidget)
				{
					if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject()))
					{
						GazeUI->SetProgress(Progress);
					}
				}
			}

			// TRIGGER PAYOFF
			if (Progress >= 1.0f)
			{
				bHasTriggered = true;
				if (SoundGazeCompleted) UGameplayStatics::SpawnSoundAttached(SoundGazeCompleted, this);

				// THE CUT-OFF: Ensure vibration cuts out sharply when gaze completes
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
				{
					PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
					PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
				}

				OnGazeTriggered.Broadcast(this);

				if (bResetOnTrigger)
				{
					CurrentChargeTime = 0.0f;
					OnGazeProgressUpdated.Broadcast(0.0f);

					if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
					{
						if (Pawn->GazeReticleWidget)
						{
							if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject())) GazeUI->SetProgress(0.0f);
						}
					}
					bHasTriggered = false;
				}
			}
		}
	}

	// --- 4. HIGHLIGHT PULSE ---

	// Check if ANY sibling is actively being looked at
	bool bAnySiblingLookedAt = false;
	TArray<UAzr_Gaze*> AllZones;
	GetOwner()->GetComponents(AllZones);
	for (UAzr_Gaze* Zone : AllZones)
	{
		
		if (Zone != this && Zone->bIsGazeEnabled && (Zone->bIsBeingLookedAt || Zone->bWasLookedAt))
		{
			bAnySiblingLookedAt = true;
			break;
		}
	}

	// Only pulse if NO zones are currently being looked at
	if (!bIsBeingLookedAt && HighlightMPC && !bAnySiblingLookedAt)
	{
		if (UWorld* World = GetWorld())
		{
			// REVERTED: Restored the Global Hive Mind Synchronization!
			float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
			float Phase = (World->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
			float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;

			UKismetMaterialLibrary::SetScalarParameterValue(World, HighlightMPC, FName("Alpha"), CurrentValue);

			bool bIsRising = (CurrentValue > LastHighlightValue);

			if (CurrentValue < 0.05f && bIsRising && !bWasRising && SoundHighlightStart)
				UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());

			if (bWasRising && !bIsRising && SoundHighlightEnd)
				UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());

			LastHighlightValue = CurrentValue;
			bWasRising = bIsRising;
		}
	}
	else if (bIsBeingLookedAt && HighlightMPC)
	{
		// Pin the global MPC to 1.0. 
		// (The siblings will have their CustomDepth turned off by the Hover logic, so only THIS one will glow!)
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), 1.0f);
	}

	// --- 5. THE MAGIC RESET ---
	bWasLookedAt = bIsBeingLookedAt;
	bIsBeingLookedAt = false;
}

// --- LIFECYCLE & HELPERS ---

void UAzr_Gaze::EnsureInitialized()
{
	if (!GetOwner()) return;

	if (!TargetMesh) TargetMesh = FindMeshByName(TargetMeshName);

	if (!TargetMesh)
	{
		TArray<UMeshComponent*> MeshComps; // <--- Changed to UMeshComponent
		GetOwner()->GetComponents<UMeshComponent>(MeshComps);
		for (UMeshComponent* Mesh : MeshComps)
		{
			if (Mesh)
			{
				TargetMesh = Mesh;
				break;
			}
		}
	}
}

// --- VISUAL HELPERS ---

void UAzr_Gaze::UpdatePointer()
{
	if (UAzr_Pointer* Pointer = FindPlayerPointer())
	{
		USceneComponent* Target = TargetMesh ? Cast<USceneComponent>(TargetMesh) : this;
		Pointer->EnablePointer_TargetComponent(Target);
	}
}

void UAzr_Gaze::UpdatePointerZOffset(bool bIsGazeMode)
{
	if (UAzr_Pointer* Pointer = FindPlayerPointer())
	{
		// Safely cast the Pointer to a Scene Component so we can alter its transform in space
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Pointer))
		{
			FVector CurrentLoc = SceneComp->GetRelativeLocation();
			// Retain the X/Y axes, but securely override the Z
			CurrentLoc.Z = bIsGazeMode ? 12.0f : 0.0f;
			SceneComp->SetRelativeLocation(CurrentLoc);
		}
	}
}



void UAzr_Gaze::ToggleHighlight(bool bState)
{
	TArray<UMeshComponent*> MeshesToHighlight;

	if (HighlightMode == EAzr_HighlightMode::TargetMeshOnly)
	{
		// Since TargetMesh is now guaranteed to be a UMeshComponent, no casting is needed
		if (TargetMesh) MeshesToHighlight.Add(TargetMesh);
	}
	else if (GetOwner())
	{
		// Highlight ALL meshes attached to the parent Actor
		GetOwner()->GetComponents(MeshesToHighlight);
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

// --- EDITOR GHOST SYSTEM ---

void UAzr_Gaze::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) GenerateIndicatorPreview();
	else ClearIndicatorPreview();
#endif
}

void UAzr_Gaze::OnComponentCreated()
{
	Super::OnComponentCreated();
#if WITH_EDITOR
	GenerateIndicatorPreview();
#endif
}

void UAzr_Gaze::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) GenerateIndicatorPreview();
#endif
}

#if WITH_EDITOR
void UAzr_Gaze::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAzr_Gaze, GazeIndicatorClass))
	{
		ClearIndicatorPreview();
		GenerateIndicatorPreview();
	}
}
#endif

void UAzr_Gaze::ClearIndicatorPreview()
{
	for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
	{
		if (IsValid(Mesh)) Mesh->DestroyComponent();
	}
	PreviewIndicatorMeshes.Empty();
}

void UAzr_Gaze::GenerateIndicatorPreview()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || IsGarbageCollecting()) return;

	if (!GazeIndicatorClass)
	{
		ClearIndicatorPreview();
		return;
	}

	if (PreviewIndicatorMeshes.Num() > 0 && IsValid(PreviewIndicatorMeshes[0]))
	{
		for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
		{
			if (Mesh && Mesh->GetAttachParent() != this)
			{
				Mesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			}
		}
		return;
	}

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GazeIndicatorClass);
	if (!BPClass) return;

	TFunction<void(USCS_Node*, FTransform)> ProcessNodes;
	ProcessNodes = [&](USCS_Node* Node, FTransform ParentTransform)
		{
			if (!Node) return;

			FTransform CurrentTransform = ParentTransform;
			if (USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(BPClass)))
			{
				CurrentTransform = Template->GetRelativeTransform() * ParentTransform;

				if (UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(Template))
				{
					FString MeshName = FString::Printf(TEXT("PreviewGaze_%s_%d"), *MeshTemplate->GetName(), FMath::Rand());
					UStaticMeshComponent* NewPreview = NewObject<UStaticMeshComponent>(this, FName(*MeshName));

					if (NewPreview)
					{
						NewPreview->CreationMethod = EComponentCreationMethod::Instance;
						NewPreview->SetFlags(RF_Transient | RF_TextExportTransient);
						NewPreview->RegisterComponent();

						NewPreview->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
						NewPreview->SetRelativeTransform(CurrentTransform);

						NewPreview->SetStaticMesh(MeshTemplate->GetStaticMesh());

						int32 MatCount = MeshTemplate->GetNumMaterials();
						for (int32 i = 0; i < MatCount; i++) NewPreview->SetMaterial(i, MeshTemplate->GetMaterial(i));

						NewPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						NewPreview->SetCastShadow(false);
						PreviewIndicatorMeshes.Add(NewPreview);
					}
				}
			}

			for (USCS_Node* Child : Node->GetChildNodes()) ProcessNodes(Child, CurrentTransform);
		};

	if (USimpleConstructionScript* SCS = BPClass->SimpleConstructionScript)
	{
		for (USCS_Node* RootNode : SCS->GetRootNodes()) ProcessNodes(RootNode, FTransform::Identity);
	}
}



UMeshComponent* UAzr_Gaze::FindMeshByName(FName Name)
{
	if (Name.IsNone()) return nullptr;
	TArray<UMeshComponent*> Comps; // <--- Changed to UMeshComponent
	GetOwner()->GetComponents(Comps);

	for (UMeshComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString()))
			return Comp;
	}
	return nullptr;
}



UAzr_Pointer* UAzr_Gaze::FindPlayerPointer() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	return nullptr;
}