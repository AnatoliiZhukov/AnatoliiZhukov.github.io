// MIT License - Copyright (c) 2026 Anatolii Zhukov


#include "VaultLocation.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

AVaultLocation::AVaultLocation() {
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SetRootComponent(SceneRoot);
	
	VaultZone = CreateDefaultSubobject<UBoxComponent>("VaultZone");
	VaultZone->SetupAttachment(SceneRoot);
	VaultZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	VaultZone->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	VaultZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VaultZone->SetCollisionObjectType(ECC_WorldDynamic);
	VaultZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	VaultZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	
	VaultCollision = CreateDefaultSubobject<UBoxComponent>("VaultCollision");
	VaultCollision->SetupAttachment(SceneRoot);
	VaultCollision->SetBoxExtent(FVector(100.f, 25.f, 50.f));
	VaultCollision->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	VaultCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VaultCollision->SetCollisionObjectType(ECC_WorldDynamic);
	VaultCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VaultCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	PointA = CreateDefaultSubobject<UArrowComponent>("PointA");
	PointA->SetupAttachment(SceneRoot);
	PointA->SetRelativeLocation(FVector(0.f, -100.f, 0.f));
	PointA->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	
	PointB = CreateDefaultSubobject<UArrowComponent>("PointB");
	PointB->SetupAttachment(SceneRoot);
	PointB->SetRelativeLocation(FVector(0.f, 100.f, 0.f));
	PointB->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
}

void AVaultLocation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AVaultLocation, bIsOccupied);
	DOREPLIFETIME(AVaultLocation, bIsOnCooldown);
	DOREPLIFETIME(AVaultLocation, CurrentDurability);
}

void AVaultLocation::Interact(AActor* Caller, const float DurabilityCost) {
	if (!HasAuthority()) return;
	
	ConsumeDurability(DurabilityCost);
}

bool AVaultLocation::TryOccupyVault(const bool bIgnoreCooldown) {
	if (!HasAuthority() || bIsOccupied || (!bIgnoreCooldown && bIsOnCooldown)) return false;
	
	bIsOccupied = true;
	return true;
}

void AVaultLocation::UnlockVault() {
	if (!HasAuthority()) return;

	bIsOccupied = false;
}

void AVaultLocation::ConsumeDurability(const float Cost) {
	if (!HasAuthority() || bIsOnCooldown || Cost <= 0.f) return;
	
	CurrentDurability = FMath::Max(CurrentDurability - Cost, 0.f);
	
	if (CurrentDurability <= 0.f) bIsOnCooldown = true;
	
	bIsOccupied = false;
	
	GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	GetWorldTimerManager().SetTimer(
		CooldownTimerHandle,
		this,
		&AVaultLocation::OnCooldownFinished,
		CooldownDuration,
		false
	);
}

UArrowComponent* AVaultLocation::GetClosestPointToActor(const AActor* Actor) const {
	if (!Actor) return nullptr;
	
	const float DistanceToA = FVector::DistSquared(Actor->GetActorLocation(), PointA->GetComponentLocation());
	const float DistanceToB = FVector::DistSquared(Actor->GetActorLocation(), PointB->GetComponentLocation());
	
	return DistanceToA <= DistanceToB ? PointA : PointB;
}

void AVaultLocation::BeginPlay() {
	Super::BeginPlay();
	
	if (HasAuthority()) CurrentDurability = MaxDurability;
}

void AVaultLocation::OnRep_IsOnCooldown_Implementation() {}

void AVaultLocation::OnCooldownFinished() {
	if (!HasAuthority()) return;
	
	CurrentDurability = MaxDurability;
	bIsOnCooldown = false;
}

