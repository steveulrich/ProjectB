#include "Buildable/BwayRawlinsJailCage.h"

#include "Abilities/BwayGameplayEffect_RawlinsJail.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayRawlinsKitConfig.h"
#include "Character/LyraHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRawlinsJailCage)

ABwayRawlinsJailCage::ABwayRawlinsJailCage()
{
	bPersistsBetweenRounds = false;
	BuildTime = 0.f;
	PrimaryActorTick.bCanEverTick = false;

	KitConfig = TSoftObjectPtr<UBwayRawlinsKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig.DA_BW_RawlinsKitConfig")));

	CageHitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CageHitSphere"));
	CageHitSphere->SetupAttachment(RootComponent);
	CageHitSphere->InitSphereRadius(CageRadius);
	CageHitSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CageHitSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CageHitSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Block WorldDynamic so Rawlins bullets (ECC_WorldDynamic + Block) stop on the cage.
	CageHitSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CageHitSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CageHitSphere->SetGenerateOverlapEvents(false);
	CageHitSphere->CanCharacterStepUpOn = ECB_No;

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	JailedEffectClass = UGE_Bway_RawlinsJailed::StaticClass();
}

void ABwayRawlinsJailCage::ApplyKitConfigOverrides()
{
	if (KitConfig.IsNull())
	{
		return;
	}

	if (const UBwayRawlinsKitConfig* Config = KitConfig.LoadSynchronous())
	{
		MaxHealth = Config->JailMaxHealth;
		CageRadius = Config->JailCageRadius;
	}
}

void ABwayRawlinsJailCage::InitializeAbilitySystem()
{
	ApplyKitConfigOverrides();

	Super::InitializeAbilitySystem();

	if (HealthSet)
	{
		HealthSet->InitMaxHealth(MaxHealth);
		HealthSet->InitHealth(MaxHealth);
		HealthSet->OnOutOfHealth.AddUObject(this, &ABwayRawlinsJailCage::HandleOutOfHealth);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ABwayRawlinsJailCage::HandleDeathStarted);
	}

	if (CageHitSphere)
	{
		CageHitSphere->SetSphereRadius(CageRadius);
	}
}

void ABwayRawlinsJailCage::BeginPlay()
{
	Super::BeginPlay();

	if (CageHitSphere)
	{
		CageHitSphere->SetSphereRadius(CageRadius);
	}
}

void ABwayRawlinsJailCage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseVictim();

	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ABwayRawlinsJailCage::HandleDeathStarted);
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayRawlinsJailCage::FinishBuilding()
{
	Super::FinishBuilding();
	// Cages are always damageable once active.
	SetCanBeDamaged(true);
}

void ABwayRawlinsJailCage::CaptureVictim(ABwayCharacterWithAbilities* InVictim)
{
	if (!HasAuthority() || !InVictim || CapturedVictim.IsValid())
	{
		return;
	}

	CapturedVictim = InVictim;

	if (UCapsuleComponent* Capsule = InVictim->GetCapsuleComponent())
	{
		AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else
	{
		AttachToActor(InVictim, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	if (UCharacterMovementComponent* Movement = InVictim->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	if (ULyraHealthComponent* VictimHealth = ULyraHealthComponent::FindHealthComponent(InVictim))
	{
		VictimHealth->OnDeathStarted.AddDynamic(this, &ABwayRawlinsJailCage::HandleVictimDeathStarted);
	}

	ApplyJailedEffect();
}

void ABwayRawlinsJailCage::ApplyJailedEffect()
{
	ABwayCharacterWithAbilities* Victim = CapturedVictim.Get();
	if (!Victim || !JailedEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	AActor* EffectInstigator = GetInstigator();
	if (!EffectInstigator)
	{
		EffectInstigator = this;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(EffectInstigator, this);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(JailedEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	JailedEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ABwayRawlinsJailCage::ReleaseVictim()
{
	if (bVictimReleased)
	{
		return;
	}
	bVictimReleased = true;

	if (ABwayCharacterWithAbilities* Victim = CapturedVictim.Get())
	{
		if (ULyraHealthComponent* VictimHealth = ULyraHealthComponent::FindHealthComponent(Victim))
		{
			VictimHealth->OnDeathStarted.RemoveDynamic(this, &ABwayRawlinsJailCage::HandleVictimDeathStarted);
		}

		if (JailedEffectHandle.IsValid())
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim))
			{
				TargetASC->RemoveActiveGameplayEffect(JailedEffectHandle);
			}
		}
	}

	JailedEffectHandle.Invalidate();
	CapturedVictim.Reset();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

void ABwayRawlinsJailCage::DestroyCage()
{
	if (!HasAuthority() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	ReleaseVictim();
	Destroy();
}

void ABwayRawlinsJailCage::HandleOutOfHealth(
	AActor* EffectInstigator,
	AActor* EffectCauser,
	const FGameplayEffectSpec* EffectSpec,
	float EffectMagnitude,
	float OldValue,
	float NewValue)
{
	(void)EffectInstigator;
	(void)EffectCauser;
	(void)EffectSpec;
	(void)EffectMagnitude;
	(void)OldValue;
	(void)NewValue;

	HandleDeathStarted(this);
}

void ABwayRawlinsJailCage::HandleDeathStarted(AActor* OwningActor)
{
	(void)OwningActor;
	DestroyCage();
}

void ABwayRawlinsJailCage::HandleVictimDeathStarted(AActor* OwningActor)
{
	(void)OwningActor;
	DestroyCage();
}
