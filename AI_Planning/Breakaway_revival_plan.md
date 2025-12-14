# Breakaway Revival Project Plan

## Overview
This document outlines the plan to revive *Breakaway*, a cancelled team-based multiplayer brawler by Amazon Game Studios, using Unreal Engine 5.5 and the Lyra Starter Project. The goal is to create a playable vertical slice of a 4v4 match with at least four playable heroes, polishing existing systems and implementing missing ones. The project is hosted at [https://github.com/steveulrich/ProjectB](https://github.com/steveulrich/ProjectB).

### Breakaway Gameplay
*Breakaway* is a 4v4 mythological brawler where two teams compete to score by:
- Moving a physics-based ball (relic) to the opponent’s goal.
- Eliminating the entire enemy team for a round win.
- Controlling the relic in the opponent’s half when the timer expires.
Each hero has unique abilities and can place two buildable structures (e.g., turrets, healing shrines, walls), upgradable with gold earned during matches. The game was designed for fast-paced action and Twitch streaming integration.

### Lyra Starter Project
The Lyra Starter Project is a sample game for Unreal Engine 5, offering:
- Multiplayer support via Epic Online Services.
- Gameplay Ability System (GAS) for character abilities.
- Modular game modes (e.g., team deathmatch, control point).
- Scalability across platforms and UE5 Mannequin characters (Manny, Quinn) compatible with MetaHuman skeletons.

Lyra’s framework is ideal for *Breakaway*’s multiplayer and hero systems but requires customization for relic mechanics, buildables, and scoring.

## Current State
The project has early prototypes of:
- **Physics Ball (Relic)**: Basic implementation, possibly unstable.
- **Hero System**: Selection UI and custom movement, partially functional.
- **Buildable Placement**: Early system, needs refinement.
- **Scoring Logic**: Implemented but may have bugs.

These systems require polishing, and additional systems (e.g., hero abilities, multiplayer synchronization) need implementation.

## Key Systems to Implement/Polish

| System | Description | Status | Priority |
|--------|-------------|--------|----------|
| Hero System | Hero selection UI, unique abilities, custom movement | Partially implemented, needs debugging | High |
| Relic Mechanics | Physics-based ball with pick-up, carry, throw, and scoring interactions | Early prototype, needs robustness | High |
| Buildable Structures | Placement and upgrading of turrets, healing shrines, walls | Early prototype, needs full implementation | Medium |
| Scoring System | Rules for scoring via relic, team elimination, or relic control | Partially implemented, needs testing | High |
| Multiplayer Networking | 4v4 match support with synchronized game state | Likely supported by Lyra, needs customization | High |
| User Interface | In-game HUD, menus for hero selection | Partially implemented, needs polish | Medium |
| Audio/Visual Effects | Effects for abilities, relic, structures | Not implemented | Low |

## Development Plan

### 1. Understand Lyra and GAS
- **Objective**: Leverage Lyra’s framework for rapid development.
- **Tasks**:
  - Study Lyra documentation ([Unreal Engine Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)).
  - Review the Gameplay Ability System (GAS) framework from the provided attachment, inspired by *Risk of Rain 2*, focusing on:
    - Character attributes (e.g., Health, Damage).
    - Skill sets and ability stacking.
    - Damage pipeline for ability interactions.
  - Verify Unreal Engine 5.5 APIs (e.g., `UGameplayAbility`, `UAbilitySystemComponent`) for compatibility.

### 2. Polish Existing Systems
- **Hero Selection UI**:
  - Debug issues in the current UI using `UUserWidget` and `UCommonActivatableWidget` (Lyra’s UI framework).
  - Ensure seamless hero selection for 4v4 matches.
- **Custom Movement**:
  - Use `ACharacter` and `UCharacterMovementComponent` to refine movement (e.g., dashes, jumps).
  - Fix bugs affecting responsiveness or multiplayer sync.
- **Scoring Logic**:
  - Test current scoring implementation for relic-to-goal scoring.
  - Debug issues using Unreal’s Gameplay Debugger.

### 3. Implement Hero Abilities
- **Objective**: Create four unique heroes with abilities using GAS.
- **Design**:
  - Example heroes (inspired by Breakaway’s mythological theme):
    - **Spartacus (Tank)**: High health, melee-focused, with a shield bash ability.
    - **Morgan Le Fay (Mage)**: Ranged magic attacks, crowd control abilities.
    - **Valkyrie (Support)**: Healing and mobility abilities.
    - **Robin Hood (Ranged)**: Precision-based ranged attacks.
  - Each hero should have 3–4 abilities (e.g., Primary, Secondary, Utility, Special).
- **Implementation**:
  - Use `UGameplayAbility` to define abilities, binding to input via `UInputAction` (Lyra’s input system).
  - Example: Spartacus’ shield bash ability:
    ```cpp
    #include "AbilitySystem/Abilities/GameplayAbility.h"
    #include "GameFramework/Character.h"
    #include "GameFramework/CharacterMovementComponent.h"

    class UShieldBashAbility : public UGameplayAbility
    {
        GENERATED_BODY()
    public:
        virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override
        {
            if (HasAuthority(&ActivationInfo))
            {
                ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
                if (Character)
                {
                    // Apply forward impulse for bash
                    FVector Forward = Character->GetActorForwardVector() * 1000.0f;
                    Character->GetCharacterMovement()->AddImpulse(Forward, true);

                    // Apply damage effect to nearby enemies
                    FGameplayEffectSpecHandle DamageEffectSpec = MakeOutgoingGameplayEffectSpec(DamageEffectClass);
                    if (DamageEffectSpec.IsValid())
                    {
                        ApplyGameplayEffectSpecToTarget(Handle, ActorInfo, ActorInfo->AvatarActor.Get(), DamageEffectSpec);
                    }
                    CommitAbility(Handle, ActorInfo, ActivationInfo);
                }
            }
        }

        UPROPERTY(EditDefaultsOnly, Category = "GameplayEffect")
        TSubclassOf<UGameplayEffect> DamageEffectClass;
    };
    ```
  - Verify APIs in Unreal Engine 5.5 documentation ([UGameplayAbility](https://docs.unrealengine.com/5.5/en-US/API/Runtime/GameplayAbilities/UGameplayAbility/)).
  - Test abilities in multiplayer to ensure replication.

### 4. Develop Relic Mechanics
- **Objective**: Create a robust physics-based relic system.
- **Implementation**:
  - Create a relic actor using `AStaticMeshActor` with a `UStaticMeshComponent` and physics enabled:
    ```cpp
    #include "GameFramework/Actor.h"
    #include "Components/StaticMeshComponent.h"

    class ARelicActor : public AActor
    {
        GENERATED_BODY()
    public:
        ARelicActor()
        {
            StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
            StaticMeshComponent->SetSimulatePhysics(true);
            StaticMeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
            RootComponent = StaticMeshComponent;
        }

        void Interact(ACharacter* Interactor)
        {
            if (Interactor && !bIsHeld)
            {
                AttachToActor(Interactor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                StaticMeshComponent->SetSimulatePhysics(false);
                bIsHeld = true;
            }
        }

        void Throw(FVector Direction, float Force)
        {
            if (bIsHeld)
            {
                DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                StaticMeshComponent->SetSimulatePhysics(true);
                StaticMeshComponent->AddImpulse(Direction * Force);
                bIsHeld = false;
            }
        }

    private:
        UPROPERTY(VisibleAnywhere)
        UStaticMeshComponent* StaticMeshComponent;

        bool bIsHeld = false;
    };
    ```
  - Implement player interaction using `IInteractableInterface` or a custom interface.
  - Handle scoring when the relic enters the goal area using `OnComponentBeginOverlap` on a `UBoxComponent`.
  - Ensure multiplayer replication using `SetReplicates(true)` and `AActor::ReplicatedMovement`.

### 5. Buildable Structures System
- **Objective**: Allow players to place and upgrade structures.
- **Design**:
  - Structure types: Turret (attacks enemies), Healing Shrine (heals allies), Wall (blocks movement).
  - Gold system: Earn gold via kills, assists, or objectives.
- **Implementation**:
  - Create a base structure actor:
    ```cpp
    #include "GameFramework/Actor.h"
    #include "Components/StaticMeshComponent.h"

    class ABuildableStructure : public AActor
    {
        GENERATED_BODY()
    public:
        ABuildableStructure()
        {
            MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
            RootComponent = MeshComponent;
            SetReplicates(true);
        }

        virtual void ActivateStructure() PURE_VIRTUAL(ABuildableStructure::ActivateStructure, );

        void Upgrade(int32 NewLevel)
        {
            Level = NewLevel;
            // Update stats (e.g., damage, health) based on Level
        }

    protected:
        UPROPERTY(VisibleAnywhere)
        UStaticMeshComponent* MeshComponent;

        UPROPERTY(Replicated)
        int32 Level = 1;
    };
    ```
  - Implement placement using a preview actor and `LineTraceSingleByChannel` for valid locations.
  - Create specific structure classes (e.g., `ATurretStructure` with AI targeting).
  - Manage gold via a `UGameplayAttribute` in the player’s `UAbilitySystemComponent`.

### 6. Scoring and Game Mode
- **Objective**: Implement Breakaway’s scoring rules.
- **Implementation**:
  - Create a custom game mode using `AGameModeBase`:
    ```cpp
    #include "GameFramework/GameModeBase.h"
    #include "RelicActor.h"

    class ABreakawayGameMode : public AGameModeBase
    {
        GENERATED_BODY()
    public:
        void ScorePoint(int32 TeamIndex)
        {
            TeamScores[TeamIndex]++;
            // Broadcast score update to clients
            OnScoreUpdated.Broadcast(TeamScores[0], TeamScores[1]);
        }

        void CheckWinConditions()
        {
            if (TeamScores[0] >= MaxScore || TeamScores[1] >= MaxScore)
            {
                EndMatch();
            }
        }

        void OnRelicScored(ARelicActor* Relic, int32 ScoringTeam)
        {
            ScorePoint(ScoringTeam);
            ResetRelic();
        }

        void OnTeamEliminated(int32 EliminatedTeam)
        {
            ScorePoint(1 - EliminatedTeam); // Opposing team scores
            ResetRound();
        }

        void OnTimerExpired()
        {
            if (Relic->GetActorLocation().X > 0) // Example: Opponent’s half
                ScorePoint(0);
            else
                ScorePoint(1);
            ResetRound();
        }

    private:
        UPROPERTY()
        TArray<int32> TeamScores = {0, 0};

        UPROPERTY(EditDefaultsOnly)
        int32 MaxScore = 5;

        UPROPERTY()
        ARelicActor* Relic;

        FDelegateHandle OnScoreUpdated;
    };
    ```
  - Handle round timers using `GetWorld()->GetTimerManager()`.

### 7. Multiplayer and Map
- **Multiplayer**:
  - Use Lyra’s Epic Online Services for 4v4 matches.
  - Test replication for relic, structures, and abilities.
- **Map**:
  - Design a symmetrical map with goals at opposite ends, using Unreal’s Level Editor.
  - Incorporate thematic elements (e.g., El Dorado-inspired visuals).

### 8. UI and Effects
- **UI**:
  - Extend Lyra’s `UCommonActivatableWidget` for HUD elements (score, timer, gold).
  - Create a hero selection menu using `UUserWidget`.
- **Effects**:
  - Use Niagara for visual effects (e.g., ability particles).
  - Add sound cues via `USoundCue` for actions.

### 9. Testing and Debugging
- **Tasks**:
  - Test single-player and multiplayer scenarios with bots (Lyra’s AI framework).
  - Use Unreal’s Gameplay Debugger and `stat net` for network performance.
  - Iterate based on playtest feedback.

### 10. Documentation
- **Tasks**:
  - Update the GitHub README with system overviews and setup instructions.
  - Use GitHub Issues for task tracking and Wikis for detailed design docs.
  - Document code using comments and maintain a changelog.

## Recommendations
- **Prioritize Core Mechanics**: Focus on one hero and relic mechanics first to ensure integration.
- **Leverage Lyra**: Use Lyra’s modular systems to reduce development time.
- **Community Engagement**: Share progress on GitHub to attract contributors.
- **Iterative Testing**: Regularly test in multiplayer to catch replication issues early.

## Resources
- [Unreal Engine 5.5 Documentation](https://docs.unrealengine.com/5.5/en-US/)
- [Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- [Breakaway Gameplay Details](https://en.wikipedia.org/wiki/Breakaway_%28cancelled_video_game%29)
- [Breakaway Gameplay Video](https://www.youtube.com/watch?v=EcAf4kXaMYA)