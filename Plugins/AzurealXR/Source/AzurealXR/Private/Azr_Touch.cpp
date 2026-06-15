

#include "Azr_Touch.h"
#include "Azr_TouchZone.h"
#include "Azr_Indicator.h"
#include "Azr_Pointer.h"
#include "Azr_Interactable.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "CableComponent.h"
#include "Azr_ExplainWidget.h"
#include "Azr_ActionWidget.h"
#include "Azr_LabelWidget.h"
#include "Engine/GameInstance.h"

UAzr_Touch::UAzr_Touch()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// --- DEFAULTS ---
	InteractID = 1;
	HighlightSpeed = 1.2f;
	StencilID = 252;
	LastValue = 0.0f;
	bWasRising = false;
	bIsTouchEnabled = false;

	// --- INTERNAL COMPONENTS ---
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
	TetherCable->NumSegments = 1;
	TetherCable->CableLength = 0.0f;

	// --- ASSET INITIALIZATION ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded()) TetherSettings.AnchorMesh = SphereMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded()) TetherSettings.CableMaterial = CableMatAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> TouchSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Grab"));
	if (TouchSoundAsset.Succeeded()) SoundOnTouch = TouchSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> UntouchSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Release"));
	if (UntouchSoundAsset.Succeeded()) SoundOnUntouch = UntouchSoundAsset.Object;
}

void UAzr_Touch::BeginPlay()
{
	Super::BeginPlay();

	EnsureInitialized();
	AutoDetectTouchZone();

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (LinkedTouchZone)
	{
		LinkedTouchZone->OnComponentBeginOverlap.AddDynamic(this, &UAzr_Touch::OnTouchZoneBeginOverlap);
		LinkedTouchZone->OnComponentEndOverlap.AddDynamic(this, &UAzr_Touch::OnTouchZoneEndOverlap);
	}
}

void UAzr_Touch::EnsureInitialized()
{
	if (!GetOwner()) return;

	if (StartAnchor->GetAttachParent() == nullptr)
	{
		StartAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	if (!TargetHandleMesh) TargetHandleMesh = FindMeshByName(TargetMeshName);

	if (!AutoDetectedMesh)
	{
		TArray<UStaticMeshComponent*> MeshComps;
		GetOwner()->GetComponents<UStaticMeshComponent>(MeshComps);
		for (UStaticMeshComponent* Mesh : MeshComps)
		{
			if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
			{
				AutoDetectedMesh = Mesh;
				break;
			}
		}
	}
}

void UAzr_Touch::AutoDetectTouchZone()
{
	if (!GetOwner()) return;

	TArray<UAzr_TouchZone*> FoundZones;
	GetOwner()->GetComponents<UAzr_TouchZone>(FoundZones);

	for (UAzr_TouchZone* Zone : FoundZones)
	{
		if (Zone && Zone->InteractID == InteractID)
		{
			LinkedTouchZone = Zone;
			break;
		}
	}
}

void UAzr_Touch::EnableTouch()
{
	if (bIsTouchEnabled) return;
	bIsTouchEnabled = true;

	EnsureInitialized();

	// 1. Wake up the matched Touch Zone
	if (LinkedTouchZone)
	{
		LinkedTouchZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		LinkedTouchZone->SetCollisionProfileName(FName("Azr_Collision"));

		if (LinkedTouchZone->SpawnedIndicator)
		{
			LinkedTouchZone->SpawnedIndicator->SetActorHiddenInGame(false);
		}

		// --- THE INITIAL SWEEP FIX ---
		// Manually query the physics engine the exact moment the box wakes up
		TArray<UPrimitiveComponent*> OverlappingComps;
		LinkedTouchZone->GetOverlappingComponents(OverlappingComps);

		for (UPrimitiveComponent* Comp : OverlappingComps)
		{
			if (Comp && (Comp->ComponentHasTag(FName("Left")) || Comp->ComponentHasTag(FName("Right"))))
			{
				ActiveHands.AddUnique(Comp);
			}
		}

		// If a hand was already inside, manually execute the entry logic immediately!
		if (ActiveHands.Num() > 0)
		{
			if (LinkedTouchZone->SpawnedIndicator) LinkedTouchZone->SpawnedIndicator->OnExpand();
			if (SoundOnTouch) UGameplayStatics::SpawnSoundAttached(SoundOnTouch, GetOwner()->GetRootComponent());

			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				bool bIsRight = ActiveHands[0]->ComponentHasTag(FName("Right"));
				PC->PlayHapticEffect(HapticOnTouch, bIsRight ? EControllerHand::Right : EControllerHand::Left, 1.5f, false);
			}

			OnTouched.Broadcast();
		}
	}

	// 2. Setup Highlighting
	if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }
	AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);

	// 3. Prepare Visuals
	// 3. Prepare Visuals
	CurrentTargetWidget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		// --- NEW: INJECT DYNAMIC TEXT FOR ANY WIDGET TYPE ---
		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			// 1. Is it an Explain Widget?
			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(TouchDescription);
			}
			// 2. Is it an Action Widget?
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(TouchDescription);
			}
			// 3. Is it a Label Widget?
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(TouchDescription);
			}
		}
	}

	ToggleHighlight(true);
	ToggleTether(true);
	UpdatePointer(false);

	SetComponentTickEnabled(true);
}

void UAzr_Touch::DisableTouch()
{
	if (!bIsTouchEnabled) return;
	bIsTouchEnabled = false;

	// 1. Sleep the matched collision
	if (LinkedTouchZone)
	{
		LinkedTouchZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (LinkedTouchZone->SpawnedIndicator)
		{
			LinkedTouchZone->SpawnedIndicator->SetActorHiddenInGame(true);
		}
	}

	// 2. Visual Cleanup
	ToggleHighlight(false);
	ToggleTether(false);

	if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();
	if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(false);

	ActiveHands.Empty();
	SetComponentTickEnabled(false);
}

// --- OVERLAP HANDLERS ---

void UAzr_Touch::OnTouchZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsTouchEnabled || !OtherComp) return;

	// Check for VR Hand Component Tags
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		// AddUnique ensures we don't double-count if the Initial Sweep already caught this hand
		ActiveHands.AddUnique(OtherComp);

		// Only fire logic on the FIRST hand entry
		if (ActiveHands.Num() == 1)
		{
			if (LinkedTouchZone && LinkedTouchZone->SpawnedIndicator)
			{
				LinkedTouchZone->SpawnedIndicator->OnExpand();
			}

			if (SoundOnTouch) UGameplayStatics::SpawnSoundAttached(SoundOnTouch, GetOwner()->GetRootComponent());

			// Strong Connection Haptics (Synced with Latch intensity)
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				bool bIsRight = OtherComp->ComponentHasTag(FName("Right"));
				PC->PlayHapticEffect(HapticOnTouch, bIsRight ? EControllerHand::Right : EControllerHand::Left, 1.5f, false);
			}

			OnTouched.Broadcast();
		}
	}
}

void UAzr_Touch::OnTouchZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!bIsTouchEnabled || !OtherComp) return;

	// SECURITY FIX: Only process objects that are confirmed VR hands
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		ActiveHands.Remove(OtherComp);

		// Only fire release logic when the zone is completely empty of valid hands
		if (ActiveHands.Num() == 0)
		{
			if (LinkedTouchZone && LinkedTouchZone->SpawnedIndicator)
			{
				LinkedTouchZone->SpawnedIndicator->OnShrink();
			}

			if (SoundOnUntouch) UGameplayStatics::SpawnSoundAttached(SoundOnUntouch, GetOwner()->GetRootComponent());

			OnUntouched.Broadcast();
		}
	}
}

void UAzr_Touch::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1. Billboard Widget & Dynamic Tether Update
	if (bIsTouchEnabled && CurrentTargetWidget)
	{
		if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			FVector StartLoc = CurrentTargetWidget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
			CurrentTargetWidget->SetWorldRotation(NewRot);
		}

		if (TetherSettings.bEnableTether && EndAnchor)
		{
			FVector DynamicEndPos = CalculateSurfaceAnchor(CurrentTargetWidget, TetherSettings.WidgetAnchorPos, TetherSettings);
			EndAnchor->SetWorldLocation(DynamicEndPos);
		}
	}

	if (bIsTouchEnabled) UpdatePointer(ActiveHands.Num() > 0);

	// 2. STABILIZED PULSE & AUDIO
	if (bIsTouchEnabled && ActiveHands.Num() == 0 && HighlightMPC)
	{
		float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
		float Phase = (GetWorld()->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
		float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), CurrentValue);

		// Rising/Falling detection for Pulse Sounds
		bool bIsRising = (CurrentValue > LastValue);
		if (CurrentValue < 0.05f && bIsRising && !bWasRising && SoundHighlightStart)
		{
			UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
		}
		if (bWasRising && !bIsRising && SoundHighlightEnd)
		{
			UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());
		}
		LastValue = CurrentValue;
		bWasRising = bIsRising;
	}
}

// --- FIXED TETHER LOGIC (MATCHES LATCH) ---

void UAzr_Touch::ToggleTether(bool bState)
{
	if (!bState || !TetherSettings.bEnableTether)
	{
		StartAnchor->SetVisibility(false); EndAnchor->SetVisibility(false); TetherCable->SetVisibility(false);
		return;
	}

	USceneComponent* MeshTarget = TargetHandleMesh ? Cast<USceneComponent>(TargetHandleMesh) : AutoDetectedMesh;
	USceneComponent* WidgetTarget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (!MeshTarget || !WidgetTarget) return;

	// Apply Designer Settings
	if (TetherSettings.AnchorMesh) { StartAnchor->SetStaticMesh(TetherSettings.AnchorMesh); EndAnchor->SetStaticMesh(TetherSettings.AnchorMesh); }
	StartAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	if (TetherSettings.CableMaterial) TetherCable->SetMaterial(0, TetherSettings.CableMaterial);
	TetherCable->CableWidth = TetherSettings.CableWidth;

	// Lazy Attachment Check (Prevents Drift)
	if (StartAnchor->GetAttachParent() != MeshTarget) StartAnchor->AttachToComponent(MeshTarget, FAttachmentTransformRules::KeepWorldTransform);
	if (EndAnchor->GetAttachParent() != WidgetTarget) EndAnchor->AttachToComponent(WidgetTarget, FAttachmentTransformRules::KeepWorldTransform);

	StartAnchor->SetWorldLocation(CalculateSurfaceAnchor(MeshTarget, TetherSettings.MeshAnchorPos, TetherSettings));
	TetherCable->SetAttachEndToComponent(EndAnchor);

	StartAnchor->SetVisibility(true); EndAnchor->SetVisibility(true); TetherCable->SetVisibility(true);
}

// --- SURFACE ANCHOR MATH (SYNCED WITH LATCH/GRAB) ---

FVector UAzr_Touch::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config)
{
	if (!Target) return FVector::ZeroVector;

	if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(Target))
	{
		FVector2D DrawSize = WidgetComp->GetCurrentDrawSize();
		FVector Scale = WidgetComp->GetComponentScale();
		FVector2D Pivot = WidgetComp->GetPivot();

		float WorldHalfWidth = (DrawSize.X * 0.5f) * Scale.Y;
		float WorldHalfHeight = (DrawSize.Y * 0.5f) * Scale.Z;

		FVector VisualCenter = WidgetComp->GetComponentLocation() +
			(WidgetComp->GetRightVector() * ((0.5f - Pivot.X) * (DrawSize.X * Scale.Y))) +
			(WidgetComp->GetUpVector() * ((0.5f - Pivot.Y) * (DrawSize.Y * Scale.Z)));

		switch (Pos)
		{
		case EAzr_TetherPos::Top:    return VisualCenter + (WidgetComp->GetUpVector() * (WorldHalfHeight + Config.WidgetGap_Vertical));
		case EAzr_TetherPos::Bottom: return VisualCenter - (WidgetComp->GetUpVector() * (WorldHalfHeight + Config.WidgetGap_Vertical));
		case EAzr_TetherPos::Right:  return VisualCenter + (WidgetComp->GetRightVector() * (WorldHalfWidth + Config.WidgetGap_Horizontal));
		case EAzr_TetherPos::Left:   return VisualCenter - (WidgetComp->GetRightVector() * (WorldHalfWidth + Config.WidgetGap_Horizontal));
		default: return VisualCenter;
		}
	}

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
	case EAzr_TetherPos::Top:    SurfacePush = UpVec * (WorldHalfHeight + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Bottom: SurfacePush = -UpVec * (WorldHalfHeight + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Right:  SurfacePush = RightVec * (WorldHalfWidth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Left:   SurfacePush = -RightVec * (WorldHalfWidth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Front:  SurfacePush = ForwardVec * (WorldHalfDepth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Back:   SurfacePush = -ForwardVec * (WorldHalfDepth + Config.MeshSurfaceOffset); break;
	default: break;
	}

	// This is the line you were missing! Adding the Vertical and Horizontal offsets to the final calculation.
	return WorldCenter + SurfacePush + (UpVec * Config.MeshOffset_Vertical) + (RightVec * Config.MeshOffset_Horizontal);
}

// --- HELPER BOILERPLATE ---

void UAzr_Touch::ToggleHighlight(bool bState)
{
	if (HighlightMode == EAzr_HighlightMode::None) return;
	TArray<UMeshComponent*> Meshes;
	if (HighlightMode == EAzr_HighlightMode::AllComponents) GetOwner()->GetComponents(Meshes);
	else if (TargetHandleMesh) Meshes.Add(Cast<UMeshComponent>(TargetHandleMesh));
	else if (AutoDetectedMesh) Meshes.Add(Cast<UMeshComponent>(AutoDetectedMesh));

	for (UMeshComponent* Mesh : Meshes) { if (Mesh) { Mesh->SetRenderCustomDepth(bState); Mesh->SetCustomDepthStencilValue(StencilID); } }
}

void UAzr_Touch::UpdatePointer(bool bForceActive)
{
	if (UAzr_Pointer* P = FindPlayerPointer())
	{
		USceneComponent* Target = TargetHandleMesh ? Cast<USceneComponent>(TargetHandleMesh) : AutoDetectedMesh;
		P->EnablePointer_TargetComponent(Target ? Target : GetOwner()->GetRootComponent());
	}
}

void UAzr_Touch::EndPlay(const EEndPlayReason::Type Reason) { DisableTouch(); Super::EndPlay(Reason); }

UPrimitiveComponent* UAzr_Touch::FindMeshByName(FName Name)
{
	TArray<UPrimitiveComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (auto* M : Comps) if (M->GetFName() == Name || M->GetName().Contains(Name.ToString())) return M;
	return nullptr;
}

USceneComponent* UAzr_Touch::FindWidgetByName(FName Name)
{
	TArray<USceneComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (auto* W : Comps) if (W->IsA(UWidgetComponent::StaticClass()) && (W->GetFName() == Name || W->GetName().Contains(Name.ToString()))) return Cast<USceneComponent>(W);
	return nullptr;
}

UAzr_Pointer* UAzr_Touch::FindPlayerPointer() const
{
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	return P ? P->FindComponentByClass<UAzr_Pointer>() : nullptr;
}