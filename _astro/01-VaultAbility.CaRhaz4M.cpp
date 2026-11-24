// MIT License - Copyright (c) 2026 Anatolii Zhukov


#include "VaultAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/ArrowComponent.h"
#include "Core/ChaseGameGameplayTags.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Actors/VaultLocation.h"
#include "Gameplay/Characters/ChaseGameCharacter.h"

UVaultAbility::UVaultAbility() {
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Prediction can break things, ServerInitiated is a safer but worse feeling option
	// *ServerInitiated would require moving ability activation into a Server RPC
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	SetAssetTags(FGameplayTagContainer(TAG_Ability_Vault));
	
	ActivationOwnedTags.AddTag(TAG_State_Character_Vaulting);
	
	ActivationBlockedTags.AddTag(TAG_State_Movement_Frozen);
	ActivationBlockedTags.AddTag(TAG_State_Character_Vaulting);
}

bool UVaultAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const {
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !VaultMontage) return false;

	const AVaultLocation* VaultLocation = GetTargetVaultFromActorInfo(ActorInfo);
	if (!IsValid(VaultLocation) || (!bIgnoreVaultCooldown && VaultLocation->IsOnCooldown())) return false;
	
	// Don't bother checking unless on the server
	if (ActorInfo && ActorInfo->IsNetAuthority() && VaultLocation->IsOccupied()) return false;
	
	return true;
}

void UVaultAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData) {
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		return EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

	ActiveCharacter = Cast<AChaseGameCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	ActiveVault = GetTargetVaultFromActorInfo(ActorInfo);
	
	if (!IsValid(ActiveCharacter) || !IsValid(ActiveVault))
		return EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

	// Server only checks and logic
	if (ActorInfo && ActorInfo->IsNetAuthority()) {
		if (!ActiveVault->TryOccupyVault(bIgnoreVaultCooldown)) return EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		// Inform the VaultLocation about successful interaction
		ActiveVault->Interact(ActiveCharacter, DurabilityCost);
	}

	AlignCharacterForVault(ActiveCharacter, ActiveVault);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		"VaultMontage",
		VaultMontage,
		MontagePlayRate
	);

	if (!MontageTask) {
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UVaultAbility::OnMontageFinishedPlaying);
	MontageTask->OnBlendOut.AddDynamic(this, &UVaultAbility::OnMontageFinishedPlaying);
	MontageTask->OnInterrupted.AddDynamic(this, &UVaultAbility::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UVaultAbility::OnMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UVaultAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled) {
	UnlockControlRotation();
	
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && IsValid(ActiveVault)) ActiveVault->UnlockVault();

	ActiveVault = nullptr;
	ActiveVaultCollision = nullptr;
	ActiveCharacter = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AVaultLocation* UVaultAbility::GetTargetVaultFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const {
	const AChaseGameCharacter* Character = Cast<AChaseGameCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Character) return nullptr;

	return Cast<AVaultLocation>(Character->GetTargetInteractable());
}

void UVaultAbility::AlignCharacterForVault(AChaseGameCharacter* Character, const AVaultLocation* Vault) {
	if (!Character || !Vault) return;

	const UArrowComponent* StartPoint = Vault->GetClosestPointToActor(Character);
	if (!StartPoint) return;

	FVector StartLocation = StartPoint->GetComponentLocation();
	StartLocation.Z += StartLocationZOffset;

	const FRotator StartRotation = FRotator(0.f, StartPoint->GetComponentRotation().Yaw, 0.f);

	Character->SetActorLocationAndRotation(StartLocation, StartRotation, false, nullptr, ETeleportType::TeleportPhysics);
	
	if (bLockControlRotation) LockControlRotation(StartRotation);
}

void UVaultAbility::LockControlRotation(const FRotator& TargetRotation) const {
	if (!bLockControlRotation || !IsValid(ActiveCharacter)) return;

	AController* ActiveController = ActiveCharacter->GetController();
	if (!IsValid(ActiveController)) return;

	ActiveController->SetControlRotation(TargetRotation);
	ActiveController->SetIgnoreLookInput(true);
}

void UVaultAbility::UnlockControlRotation() const {
	if (!bLockControlRotation || !IsValid(ActiveCharacter)) return;

	AController* ActiveController = ActiveCharacter->GetController();
	if (!IsValid(ActiveController)) return;
	
	ActiveController->SetIgnoreLookInput(false);
}

void UVaultAbility::OnMontageFinishedPlaying() {
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UVaultAbility::OnMontageCancelled() {
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

