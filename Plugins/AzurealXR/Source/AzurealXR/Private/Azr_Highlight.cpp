

#include "Azr_Highlight.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Azr_Interactable.h" // <--- Required for Hive Mind Access

UAzr_Highlight::UAzr_Highlight()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	StencilID = 252;
	HighlightSpeed = 1.2f;
	TargetMeshName = NAME_None;

	LastValue = 0.0f;
	bWasRising = false;

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;
}

void UAzr_Highlight::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();
	ToggleVisuals(false);
}

void UAzr_Highlight::EnsureInitialized()
{
	if (!GetOwner()) return;

	if (!AutoDetectedMesh)
	{
		AutoDetectedMesh = FindMeshByName(FName("Target Mesh"));
		if (!AutoDetectedMesh)
		{
			AutoDetectedMesh = Cast<UPrimitiveComponent>(GetOwner()->GetComponentByClass(UStaticMeshComponent::StaticClass()));
		}
	}

	if (TargetMeshName.IsNone()) CurrentTargetMesh = AutoDetectedMesh;
	else
	{
		CurrentTargetMesh = FindMeshByName(TargetMeshName);
		if (!CurrentTargetMesh) CurrentTargetMesh = AutoDetectedMesh;
	}
}

void UAzr_Highlight::EnableHighlight()
{
	EnsureInitialized();

	if (IsComponentTickEnabled()) return;

	SetComponentTickEnabled(true);

	// HIVE MIND UPDATE:
	// Since we just turned ON, we force the whole world to sync to OUR speed.
	AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);

	// SYNC FIX: Jump onto the global wave
	LastValue = 0.0f;
	bWasRising = true;

	ToggleVisuals(true);
}

void UAzr_Highlight::DisableHighlight()
{
	if (!IsComponentTickEnabled()) return;

	SetComponentTickEnabled(false);
	ToggleVisuals(false);

	if (HighlightMPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), 0.0f);
	}
}

void UAzr_Highlight::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (HighlightMPC)
	{
		// HIVE MIND READ:
		// We use the Global Speed so we match everyone else in the world.
		float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();

		// GLOBAL VISUAL SYNC
		float WorldTime = GetWorld()->GetTimeSeconds();
		float Phase = (WorldTime * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
		float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;

		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), CurrentValue);

		// AUDIO SYNC
		bool bIsRising = (CurrentValue > LastValue);

		if (CurrentValue < 0.05f && bIsRising)
		{
			if (!bWasRising)
			{
				if (SoundHighlightStart)
					UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
			}
		}

		if (bWasRising && !bIsRising)
		{
			if (SoundHighlightEnd)
				UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());
		}

		LastValue = CurrentValue;
		bWasRising = bIsRising;
	}
}

void UAzr_Highlight::ToggleVisuals(bool bState)
{
	if (HighlightMode == EAzr_HighlightMode::None) return;

	TArray<UMeshComponent*> MeshesToHighlight;

	if (HighlightMode == EAzr_HighlightMode::AllComponents)
		GetOwner()->GetComponents(MeshesToHighlight);
	else if (HighlightMode == EAzr_HighlightMode::TargetMeshOnly)
	{
		if (UMeshComponent* Target = Cast<UMeshComponent>(CurrentTargetMesh))
			MeshesToHighlight.Add(Target);
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

UPrimitiveComponent* UAzr_Highlight::FindMeshByName(FName Name)
{
	TArray<UPrimitiveComponent*> Comps;
	GetOwner()->GetComponents(Comps);
	for (UPrimitiveComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name) return Comp;
		if (Comp->GetName().Contains(Name.ToString())) return Comp;
	}
	return nullptr;
}