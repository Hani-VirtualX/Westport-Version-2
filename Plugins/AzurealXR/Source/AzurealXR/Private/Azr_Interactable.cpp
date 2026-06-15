

#include "Azr_Interactable.h"
#include "Azr_Pointer.h" 
#include "Azr_Indicator.h" 
#include "Kismet/GameplayStatics.h" 
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h" 
#include "TimerManager.h" 

// --- HIVE MIND INITIALIZATION ---
// Defaults to 1.2f. This lives in global memory.
float AAzr_Interactable::GlobalHiveSpeed = 1.2f;

void AAzr_Interactable::SetGlobalHiveSpeed(float NewSpeed)
{
	GlobalHiveSpeed = NewSpeed;
}

float AAzr_Interactable::GetGlobalHiveSpeed()
{
	return GlobalHiveSpeed;
}
// --------------------------------

AAzr_Interactable::AAzr_Interactable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bForceDisableShadows = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	GrabZone = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabZone"));
	GrabZone->SetupAttachment(SceneRoot);
	GrabZone->SetBoxExtent(FVector(10.f, 10.f, 10.f));
	GrabZone->SetCollisionProfileName(FName("Azr_Collision"));

	LeftSnap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftSnap_Visual"));
	LeftSnap->SetupAttachment(SceneRoot);
	LeftSnap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftSnap->SetCastShadow(false);
	LeftSnap->SetHiddenInGame(true);
	LeftSnap->SetRelativeRotation(FRotator(-180.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftHandMeshAsset(TEXT("/AzurealXR/Core/XRMannequins/LeftHand"));
	if (LeftHandMeshAsset.Succeeded()) LeftSnap->SetStaticMesh(LeftHandMeshAsset.Object);

	RightSnap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightSnap_Visual"));
	RightSnap->SetupAttachment(SceneRoot);
	RightSnap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightSnap->SetCastShadow(false);
	RightSnap->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RightHandMeshAsset(TEXT("/AzurealXR/Core/XRMannequins/RightHand"));
	if (RightHandMeshAsset.Succeeded()) RightSnap->SetStaticMesh(RightHandMeshAsset.Object);
}

void AAzr_Interactable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TArray<UActorComponent*> AllComps;
	GetComponents(AllComps);

	for (UActorComponent* Comp : AllComps)
	{
		if (!Comp) continue;

		if (!Comp->ComponentTags.Contains(FName("KeepTick")))
		{
			// EXCLUSION FIX: Disables tick for Scene Components, but spares Widget Components
			// so they can successfully render in the Editor Viewport!
			if (Comp->IsA(USceneComponent::StaticClass()) && !Comp->IsA(UWidgetComponent::StaticClass()))
			{
				Comp->SetComponentTickEnabled(false);
			}
		}

		if (bForceDisableShadows)
		{
			if (Comp->ComponentTags.Contains(FName("KeepShadow"))) continue;

			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
			{
				if (PrimComp->CastShadow)
				{
					PrimComp->SetCastShadow(false);
					PrimComp->MarkRenderStateDirty();
				}
			}
		}
	}
}

void AAzr_Interactable::BeginPlay()
{
	if (GrabZone) GrabZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (LeftSnap) LeftSnap->SetVisibility(false);
	if (RightSnap) RightSnap->SetVisibility(false);

	// This ensures the widgets you see in the editor immediately hide themselves 
	// the second the game starts, staying invisible until called.
	TArray<UWidgetComponent*> Widgets;
	GetComponents(Widgets);
	for (UWidgetComponent* Widget : Widgets) if (Widget) Widget->SetVisibility(false);

	if (IndicatorClass && GrabZone)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnedIndicator = GetWorld()->SpawnActor<AAzr_Indicator>(IndicatorClass, GrabZone->GetComponentTransform(), SpawnParams);
		if (SpawnedIndicator)
		{
			SpawnedIndicator->AttachToComponent(GrabZone, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SpawnedIndicator->SetActorHiddenInGame(true);
		}
	}

	Super::BeginPlay();

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (GrabZone)
	{
		GrabZone->OnComponentBeginOverlap.AddDynamic(this, &AAzr_Interactable::OnGrabZoneBeginOverlap);
		GrabZone->OnComponentEndOverlap.AddDynamic(this, &AAzr_Interactable::OnGrabZoneEndOverlap);
	}
}

USceneComponent* AAzr_Interactable::GetSnapPoint(bool bIsRightHand) const
{
	return bIsRightHand ? RightSnap : LeftSnap;
}

void AAzr_Interactable::OnGrabZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!SpawnedIndicator || !OtherComp) return;

	// 1. Verify this is actually a VR Hand
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		// 2. Count how many valid hands are currently inside the box
		int32 HandCount = 0;
		TArray<UPrimitiveComponent*> OverlappingComps;
		GrabZone->GetOverlappingComponents(OverlappingComps);

		for (UPrimitiveComponent* Comp : OverlappingComps)
		{
			if (Comp && (Comp->ComponentHasTag(FName("Left")) || Comp->ComponentHasTag(FName("Right"))))
			{
				HandCount++;
			}
		}

		// 3. Only expand if this is the very first hand entering
		if (HandCount == 1)
		{
			SpawnedIndicator->OnExpand();
		}
	}
}

void AAzr_Interactable::OnGrabZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!SpawnedIndicator || !OtherComp) return;

	// 1. Verify a VR Hand is what just left
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		// 2. Count how many valid hands are STILL inside the box
		int32 HandCount = 0;
		TArray<UPrimitiveComponent*> OverlappingComps;
		GrabZone->GetOverlappingComponents(OverlappingComps);

		for (UPrimitiveComponent* Comp : OverlappingComps)
		{
			if (Comp && (Comp->ComponentHasTag(FName("Left")) || Comp->ComponentHasTag(FName("Right"))))
			{
				HandCount++;
			}
		}

		// 3. Only shrink if the box is now completely empty of hands
		if (HandCount == 0)
		{
			SpawnedIndicator->OnShrink();
		}
	}
}

// --- POINTER LOGIC ---

void AAzr_Interactable::EnablePointer(USceneComponent* Target)
{
	GetWorld()->GetTimerManager().ClearTimer(PointerDelayHandle);

	TWeakObjectPtr<AAzr_Interactable> WeakThis(this);
	TWeakObjectPtr<USceneComponent> WeakTarget(Target);

	GetWorld()->GetTimerManager().SetTimer(PointerDelayHandle, FTimerDelegate::CreateLambda([WeakThis, WeakTarget]()
		{
			if (AAzr_Interactable* StrongThis = WeakThis.Get())
			{
				UAzr_Pointer* Pointer = StrongThis->FindPlayerPointer();

				if (Pointer)
				{
					USceneComponent* TargetComp = WeakTarget.Get();
					USceneComponent* FinalTarget = TargetComp ? TargetComp : StrongThis->SceneRoot;
					Pointer->EnablePointer_TargetComponent(FinalTarget);
				}
				else
				{
					if (APawn* NewPawn = UGameplayStatics::GetPlayerPawn(StrongThis, 0))
					{
						StrongThis->CachedPlayerPawn = NewPawn;
						if (UAzr_Pointer* LatePointer = NewPawn->FindComponentByClass<UAzr_Pointer>())
						{
							USceneComponent* TargetComp = WeakTarget.Get();
							USceneComponent* FinalTarget = TargetComp ? TargetComp : StrongThis->SceneRoot;
							LatePointer->EnablePointer_TargetComponent(FinalTarget);
						}
					}
				}
			}
		}), 0.2f, false);
}

void AAzr_Interactable::DisablePointer()
{
	GetWorld()->GetTimerManager().ClearTimer(PointerDelayHandle);

	if (UAzr_Pointer* Pointer = FindPlayerPointer())
	{
		Pointer->DisablePointer();
	}
}

UAzr_Pointer* AAzr_Interactable::FindPlayerPointer()
{
	if (CachedPlayerPawn)
	{
		return CachedPlayerPawn->FindComponentByClass<UAzr_Pointer>();
	}

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		CachedPlayerPawn = PlayerPawn;
		return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	}

	return nullptr;
}