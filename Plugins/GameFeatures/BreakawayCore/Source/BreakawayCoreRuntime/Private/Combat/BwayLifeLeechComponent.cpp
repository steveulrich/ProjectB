#include "Combat/BwayLifeLeechComponent.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "LyraGameplayTags.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayLifeLeechComponent)

void UBwayLifeLeechComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && UGameplayMessageSubsystem::HasInstance(this))
	{
		DamageListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener(
			TAG_Lyra_Damage_Message, this, &ThisClass::OnDamageMessage);
	}
}

void UBwayLifeLeechComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DamageListenerHandle.Unregister();
	Super::EndPlay(EndPlayReason);
}

void UBwayLifeLeechComponent::OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	ABwayPlayerState* Owner = Cast<ABwayPlayerState>(GetOwner());
	if (!Owner || !Owner->HasAuthority() || bApplyingLeech || Channel != TAG_Lyra_Damage_Message
		|| !FMath::IsFinite(Message.Magnitude) || Message.Magnitude <= 0.0) return;

	const ABwayPlayerState* Source = Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator));
	// Targets must directly identify a player. Do not resolve a buildable's owner as its victim.
	const ABwayPlayerState* Target = Cast<ABwayPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Target));
	if (Source != Owner || !Target || Target == Owner || Target->GetWorld() != GetWorld()) return;
	const ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	const int32 SourceTeam = GS ? GS->GetPlayerTeam(Owner) : INDEX_NONE;
	const int32 TargetTeam = GS ? GS->GetPlayerTeam(Target) : INDEX_NONE;
	if (SourceTeam == INDEX_NONE || TargetTeam == INDEX_NONE || SourceTeam == TargetTeam) return;

	ULyraAbilitySystemComponent* ASC = Owner->GetLyraAbilitySystemComponent();
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	const UBwayHeroAttributeSet* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
	APawn* Avatar = ASC ? Cast<APawn>(ASC->GetAvatarActor()) : nullptr;
	if (!IsValid(Avatar) || Avatar->IsActorBeingDestroyed() || !Health || !Hero
		|| ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)
		|| !FMath::IsFinite(Health->GetHealth()) || Health->GetHealth() <= 0.0f
		|| !FMath::IsFinite(Health->GetMaxHealth()) || !FMath::IsFinite(Hero->GetLifeLeech())) return;

	const float Fraction = FMath::Clamp(Hero->GetLifeLeech(), 0.0f, 1.0f);
	const double MissingHealth = FMath::Max(0.0, static_cast<double>(Health->GetMaxHealth()) - Health->GetHealth());
	// The damage message contains actual post-clamp health loss, never requested damage or rounded UI numbers.
	const float Healing = static_cast<float>(FMath::Min(MissingHealth, Message.Magnitude * Fraction));
	if (!FMath::IsFinite(Healing) || Healing <= 0.0f) return;

	TGuardValue<bool> ApplyingGuard(bApplyingLeech, true);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Owner, Avatar);
	FGameplayEffectSpec Spec(GetDefault<UBwayLifeLeechHealingEffect>(), Context, 1.0f);
	Spec.SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, Healing);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

UBwayLifeLeechHealingEffect::UBwayLifeLeechHealingEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat Value;
	Value.DataTag = LyraGameplayTags::SetByCaller_Heal;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = ULyraHealthSet::GetHealingAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Value);
}
