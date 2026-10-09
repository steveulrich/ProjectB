# Unreal Engine best practices bible

This document is the project’s working guide for architecture, gameplay systems, content organization, networking, debugging, and development workflow. It synthesizes the durable ideas in [Quod Soler’s combat architecture notes](https://www.quodsoler.com/), [X157’s UE5 and Lyra development notes](https://x157.github.io/), and [Unreal Directive’s guides and reference hubs](https://unrealdirective.com/).

Research date: 2026-09-06

The source material spans several Unreal Engine versions. Treat the project’s current engine branch, generated headers, compiler errors, and verified API behavior as the final authority. Use this document to make design decisions and to identify what must be verified before implementation.

## How to use this document

Use the following priority order:

1. The user’s current request and acceptance criteria.
2. The project’s existing source, rules, plugins, and feature contracts.
3. The actual engine version and API available in the checkout.
4. This bible and its linked source material.
5. Personal preference.

Use these terms consistently:

- **Must** means a correctness, safety, lifecycle, or project-integration requirement.
- **Should** means the project default unless a documented constraint justifies an exception.
- **May** means an option whose tradeoffs must be evaluated for the feature.

## The decision hierarchy

Before selecting a class, subsystem, asset type, or communication method, answer these questions in order:

1. **Who owns the state?** Identify the authority, lifetime, persistence across respawn, and destruction path.
2. **Who may change the state?** Define server, owning client, simulated client, editor, and asynchronous worker responsibilities.
3. **What is the data contract?** Separate configuration, runtime state, commands, events, and presentation payloads.
4. **What is the smallest composable boundary?** Prefer a component, ability, effect, tag, subsystem, or feature action when it owns a coherent responsibility.
5. **How will the behavior be observed?** Add logs, Gameplay Debugger or Visual Logger output, validation, and reproducible test steps before tuning.
6. **What evidence justifies optimization?** Profile the real bottleneck and record the measurement before changing a system for performance.

This order prevents a common failure mode: choosing an implementation because it is familiar before deciding who owns the behavior and how it must replicate.

## Project architecture

### Use Lyra as an integration framework

For Lyra-based work, extend the framework instead of creating a parallel game architecture.

- Keep the base Lyra namespace and source changes minimal so engine and Lyra updates remain mergeable.
- Put feature content, configuration, UI, input, abilities, effects, and feature-specific assets in Game Feature Plugins (GFPs).
- Put reusable or foundational C++ in a regular runtime plugin or an appropriate module. Keep GFP C++ focused on feature integration.
- Wire features through Experiences and Game Feature Actions. Treat the Experience as the map-scoped composition contract.
- Prefer modular gameplay injection over editing every actor class. Actors that accept injected components must opt into the framework.
- Use `IGameFrameworkInitStateInterface` when components depend on asynchronous or cross-system initialization. Make each component’s ready and failure conditions explicit.
- Keep actors thin. Let components, abilities, subsystems, and data assets own focused behavior; let actors coordinate their parts.
- Use one source of truth for a rule or piece of data. Do not maintain parallel Lyra and project implementations without a clear migration boundary.

For this project, organize feature-specific hero, relic, buildable, combat, and UI content around GFP boundaries. Keep shared gameplay contracts and reusable runtime code in the project’s regular plugins, and use the existing Breakaway architecture as the integration surface.

### Choose a system by lifetime and ownership

Use the system that matches the state rather than forcing every problem into an Actor or Blueprint.

- **Actor:** a world entity with transform, collision, replication, or level presence.
- **Actor Component:** behavior owned by an Actor and sharing its lifetime.
- **Object or data asset:** reusable state or configuration that does not need a transform.
- **Primary data asset:** discoverable, streamable content managed by the Asset Manager.
- **Subsystem:** a service whose lifetime belongs to the engine, game instance, world, local player, or editor.
- **Game Feature Action:** feature activation and deactivation work owned by a GFP.
- **Gameplay Ability:** an activatable, permission-checked action with explicit lifecycle and network policy.
- **Gameplay Effect:** a data-driven attribute, tag, ability-grant, or additional-effect change.
- **Gameplay Cue:** audiovisual or client-facing feedback associated with gameplay state or an event.
- **Mass processor:** batched simulation over data-oriented entity fragments.

Use the smallest owner that can express the lifecycle. A subsystem should not become a global bag of unrelated state, and a Level Blueprint should not become the home for reusable gameplay logic.

### Separate layers of meaning

Keep these categories distinct:

- **Configuration:** data assets, data tables, class defaults, and designer-authored values.
- **Persistent runtime state:** replicated properties, attributes, save data, or authoritative subsystem state.
- **Commands:** input or requested actions that may be rejected.
- **Events:** facts that happened, usually carrying a payload such as target, direction, or hit context.
- **Presentation:** animation, cues, UI, hitstop, camera response, sound, and local prediction.

Do not encode mutable state in asset names, Gameplay Tag strings, UI visibility, or animation section names. Do not make a presentation event the authoritative source of gameplay truth.

## Gameplay Ability System

### Use GAS where it provides a real contract

Use GAS when the behavior needs replicated numeric state, tag-based gating, prediction, ability ownership, effect duration, or consistent server and client execution. Do not use GAS for a purely local UI toggle or a one-off non-replicating interaction that has no ability or effect semantics.

The normal flow is:

`input -> ASC -> ability -> effect -> attribute or tag -> cue or event`

Tags gate the transitions. Effects and attributes hold gameplay state. Cues and events communicate consequences without becoming the source of truth.

### Decide ASC ownership deliberately

Choose Pawn or PlayerState ownership from the required lifetime.

- Put the ASC on the Pawn when abilities and attributes reset with the character and AI should use the same path.
- Put the ASC on the PlayerState when progression or abilities must survive pawn replacement and respawn.
- If the ASC is on PlayerState, implement owner and avatar repointing deliberately. Validate initialization order, replication, and late avatar assignment.

Document what survives death, what survives possession, and what is reconstructed from data.

### Treat ability lifecycle as a contract

Every ability must have a clear activation, commit, cancellation, cleanup, and end path.

- Choose the instancing policy from the ability’s state and concurrency needs. A stateful ability should not be `NonInstanced`.
- Choose the net execution policy from the authority and prediction contract. `LocalPredicted`, `LocalOnly`, `ServerOnly`, and `ServerInitiated` have different responsibilities.
- Call `CommitAbility` when the ability consumes cost, cooldown, or other commit requirements.
- Ensure every successful and failed latent path eventually calls `EndAbility`.
- Remove delegates, tasks, timers, temporary tags, spawned actors, and cached references on every terminal path.
- Do not call a Lyra ability parent implementation blindly when it has project-specific commit or lifecycle behavior. Inspect the parent contract first.
- Use `GiveAbilityAndActivateOnce` for a one-shot server-authoritative command that should grant, activate, and remove itself. Use a normal grant for a permanent ability and a Gameplay Effect ability grant for an effect-scoped ability.

### Use effects for attribute changes and timed state

Attributes change through Gameplay Effects. Do not write an attribute directly to bypass modifiers, replication, prediction, cues, or the project’s damage contract.

Choose the effect duration from the state’s semantics:

- **Instant:** one application that changes a base or meta attribute immediately.
- **Has Duration:** temporary current-value modification or tag state.
- **Infinite:** state remains until an explicit removal or condition ends it.
- **Periodic:** a duration or infinite effect executes at intervals, commonly for damage-over-time or healing-over-time.

Use an Infinite effect when an animation notify state owns the lifetime of a window. Do not create a second timer in the ability or effect to compete with the animation. Store the active effect handle when exact removal matters, especially when multiple windows can overlap.

Use Gameplay Effect Components for modular requirements, target tags, ongoing and removal requirements, chance to apply, granted abilities, and additional effects when the engine version supports them. Keep duration and stacking policy on the effect itself.

### Choose the GAS calculation by output shape

Select the least powerful calculation that produces the required output:

- `ScalableFloat` for a tunable constant.
- Attribute-based modifier for direct source or target attribute relationships.
- Modifier Magnitude Calculation (MMC) for custom math that produces one magnitude.
- Execution Calculation for custom logic that writes multiple attributes in a defined order.

For every capture, document the attribute, Source or Target ASC, and snapshot timing. A snapshot reads at spec creation; a non-snapshot capture reads the current value during evaluation.

Always populate source and target tag evaluation parameters. A tag-gated modifier that receives empty tag containers may silently produce the wrong result.

Keep MMCs pure. Use long-lived dependency delegates only for duration or infinite effects whose magnitude must react to an external change.

Use `SetByCaller` values for attack-specific or weapon-specific inputs so one effect definition can serve many attacks. Do not create a new effect asset for every numeric variation.

For damage, prefer a stable pipeline:

1. The attack supplies context and `SetByCaller` inputs.
2. The execution calculates and outputs a meta attribute such as `Damage`.
3. The AttributeSet consumes the meta attribute in `PostGameplayEffectExecute`.
4. The AttributeSet clamps and writes the real health value, resets the meta attribute, and emits the project’s consequence cue or event.
5. Death handling runs through its own authority and lifecycle path.

Do not apply new effects from inside `PostGameplayEffectExecute`. Hand off to a separate event or authority-owned system.

Remember that stack count can multiply execution output unless the execution explicitly handles stack count.

### Use tags as contracts

Use Gameplay Tags for hierarchical, growing, designer-facing, or cross-system state. Use enums for a small, fixed, exhaustive set that needs enum-specific behavior. Use a Gameplay Tag Container for sets of tags rather than an array of individual tags.

Project tag rules:

- Use PascalCase dot-separated names such as `Ability.Combat.Attack`, `State.Combat.Invulnerable`, `Event.Combat.Hit`, and `InputTag.Combat.LightAttack`.
- Prefer three levels, use four when it clarifies ownership, and split a taxonomy that needs five or more levels.
- Keep roots coherent: `Ability`, `Cooldown`, `Damage`, `Event`, `GameplayCue`, `InputTag`, `Movement`, `SetByCaller`, `State`, `Status`, `UI`, and `Weapon` are useful examples.
- Register tags in the project’s canonical configuration or native tag declarations. Keep domain files and comments near the declaration.
- Request native or configured tags once and cache them when used in hot paths.
- Use restricted roots for contracts that only a designated team or module may extend.
- Review additions for duplicate meaning. Do not encode numeric data, object identity, or transient payload values in a tag string.
- Use exact or hierarchical matching intentionally. A parent tag requirement can affect every child tag.

The same tag vocabulary should connect GAS, animation, Common UI, Enhanced Input, and feature activation without making any one of those systems own unrelated state.

### Keep prediction and feedback separate

The server owns authoritative outcomes such as damage, health, death, inventory, and resource consumption. The owning client may predict responsiveness where the ability and effect policy support it. Simulated clients consume replicated state and cues.

For predicted attacks, trigger contact feedback from the contact event or Gameplay Cue instead of waiting for replicated health. That keeps hit VFX, SFX, hitstop, camera response, and numbers responsive while the server remains authoritative.

## Animation and combat

### Let animation own timing and gameplay own authority

Animation notifies are the right place to define timing windows such as invulnerability, hitboxes, input buffering, and damage windows. The server-side gameplay pipeline must still validate the outcome.

For an i-frame window, use a generic notify state that applies an Infinite Gameplay Effect on begin and removes that exact active effect on end. Give the effect a tag such as `State.Combat.Invulnerable`. A tighter nested window can add `State.Combat.PerfectDodge`.

The damage pipeline checks the authoritative tag and emits a hit-negated event. The ability may react to that event, but the animation window remains the timing authority. Define a policy for client and server window skew so local visuals do not promise an outcome the server will reject.

### Play montages through the ASC

Use `UAbilityTask_PlayMontageAndWait` from the ability, or the project’s equivalent GAS task, for ability montages. In C++, explicitly call `ReadyForActivation()` after configuring the task.

Play the montage through the ASC, not by calling `Montage_Play` directly on the AnimInstance. The ASC path participates in `FGameplayAbilityRepAnimMontage` replication, including the montage, rate, blend data, section state, and correction information needed by remote clients.

Handle all task terminal paths:

- `OnBlendOut` means blending has begun; it does not mean the montage completed.
- `OnCompleted` means the montage finished without interruption.
- `OnInterrupted` and `OnCancelled` need cleanup.
- An explicit `EndAbility` can make an otherwise normal montage appear interrupted.

Use ASC montage section functions for combo jumps and next-section changes. If a client does not see a replicated montage, inspect avatar and mesh readiness, AnimInstance assignment, and `IsReadyForReplicatedMontage` before changing replication code.

### Make hit detection stable and data-driven

During a damage notify window, sweep capsules from each socket’s previous pose to its current pose. Use one-bone capsules for local coverage and bone-to-bone capsules for limbs or weapons. This is stable, cheap, and less dependent on per-triangle collision than overlap events.

Store hitbox rank and group in weapon or attack data. Use group to control which related hitboxes may win and rank to select a sweet spot without adding weapon-specific branching code. Define multi-hit and deduplication policy explicitly; deduplicate actors before applying area damage.

Validate collision setup before tuning traces:

- The component must be query-enabled (`QueryOnly` or `QueryAndPhysics`).
- Object channels and trace channels are different families. Use the one expected by the query.
- Ignore the attacker, owner, and instigator when appropriate.
- Use named collision profiles and custom channels for project contracts.
- Debug with the Collision Analyzer and relevant `show Collision` commands.

### Align attacks with motion warping carefully

Use Motion Warping when a root-motion attack or traversal animation must align with a dynamic target. Define a warp target, configure the Motion Warping component and notify state, and verify root-motion extraction, rotation, collision, and network authority. Treat warping as movement behavior that must be validated against ledges, walls, target movement, and interruption.

### Buffer combat input at the player boundary

The PlayerController owns the player’s input intent, so it should own the combat buffer rather than the possessed character.

- Try live event activation first.
- If `ASC->HandleGameplayEvent` activates an ability, clear stale buffered input.
- Queue only while the animation-owned buffer gate tag is active.
- Store event tag, allowed ability tag, payload, and press timestamp.
- Drain on the owning client, oldest first, at most one press per window, and drop the rest.
- Use real time for expiration so hitstop and slow motion do not create unexpectedly long buffers. A combat-specific limit around 0.4 seconds is a starting point, not a universal constant.
- Refresh directional payload at consume time when the current input is meaningful; otherwise keep the press-time direction.

Consume by matching ability tags hierarchically, and use gameplay events when a command needs a payload.

### Make AI decisions discrete and coordinated

Store dodge probability as a GAS attribute so effects can modify it. A behavior-tree decorator or equivalent decision node should roll once when the dodge decision is committed, not once per frame while an attack is traveling. Throttle repeated rolls in node memory.

Separate “should this enemy evade this attack?” from the director’s “which enemy gets to act?” Use attack tokens or engagement slots to prevent every enemy from starting a duel at once.

Model pressure as a timed, stacking effect when it represents accumulated combat pressure. Keep dodge movement relative to facing, sweep before movement, and reject paths that fall off a ledge or enter an obstacle. Missing required dodge data should fail the decision clearly.

## Input, UI, and interaction

### Treat Enhanced Input as an action contract

An Input Action defines the value and its modifiers, triggers, and listeners. An Input Mapping Context binds platform-specific keys or devices to those actions. Keep those responsibilities separate.

- Use separate contexts for gameplay, UI, spectator, vehicle, mobile, or platform-specific mappings.
- Add and remove contexts through the Enhanced Input Local Player Subsystem, commonly from the PlayerController or local-player feature layer.
- Use priorities and consumption intentionally. Higher-priority contexts should not accidentally starve gameplay input.
- Optimize continuously triggered input; do not run expensive work on every input update without evidence that it is needed.
- Use the project’s `InputTag.*` vocabulary when input must cross into Lyra, GAS, or feature code.
- Organize input assets under a stable feature or framework path and use consistent `IA_` and `IMC_` naming.
- Debug with `showdebug enhancedinput` and `showdebug devices`.

### Let Common UI own UI input and layers

Common UI should own UI input routing, activation, focus, and platform behavior. Do not build a second input router in individual widgets.

- Use Activatable Widgets for screens and modal experiences that own activation state.
- Define layer contracts such as `UI.Layer.Game`, `UI.Layer.GameMenu`, `UI.Layer.Menu`, and `UI.Layer.Modal`.
- Put a widget in the highest-priority visible layer that should receive focus.
- Configure platform input data and action display through the Common UI data assets and tables.
- Use shared button, border, and text styles rather than styling every widget independently.
- Use UI Extension Points when a generic HUD or screen needs a feature-selected widget. Let the Experience or GFP supply the implementation.
- Use `CommonUI.DumpActivatableTree` when focus or activation order is unclear.

## Replication and multiplayer

Start every replicated feature with an ownership diagram. For each value, record its authority, owning connection, relevancy, update frequency, prediction policy, and late-join behavior.

- Replicate state that remote clients need to reconstruct the game; do not replicate every local implementation detail.
- Use RepNotify when a state transition needs client-side reaction. Ensure the notification path is valid for initial replication and late join.
- Implement `GetLifetimeReplicatedProps` for every replicated property and verify conditions such as owner-only, skip-owner, initial-only, simulated-only, or custom relevance.
- Use reliable RPCs for infrequent, must-arrive commands. Do not send reliable RPCs every Tick or for high-frequency telemetry.
- Use unreliable RPCs for frequent transient feedback when loss is acceptable and the authoritative state will correct the result.
- Mark push-model properties dirty when changing them.
- Quantize replicated vectors and choose update rates from measured gameplay needs.
- Keep server validation at the boundary. A client request is a proposal, not proof that the action occurred.
- Make relevancy and cull distance part of the design. A correct replicated property is still invisible when the actor is not relevant.
- Keep client-only camera, UI, hitstop, and local prediction out of authoritative replicated state.

## Data, assets, code, and modules

### Prefer data-driven variation

Use Data Assets or Data Tables when designers need to vary content without changing gameplay code. Use a Data Asset when a row represents a cohesive object with references and behavior configuration; use a Data Table when many homogeneous rows need bulk editing or lookup. Use a Primary Data Asset when Asset Manager discovery and streaming matter.

Validate required references, ranges, tags, class types, and cross-asset relationships in editor and during load. A feature that fails because a designer left a reference empty should report the asset and property, not crash later during combat.

Use soft references when loading should be deferred, optional, or streamable. Use hard references when the dependency is required and should be resident with the owner. Inspect reference chains before deciding; a soft pointer does not help if another asset already hard-loads the same content.

Use Data Tables as data, not as a replacement for a real object model. Give rows stable names, validate row contents, and keep table ownership clear.

### Follow naming and folder contracts

Use PascalCase for identifiers and the Unreal type prefixes `U`, `A`, `F`, `S`, `E`, `T`, and `I`. Prefix booleans with `b`. Name functions as verb-noun operations such as `Get`, `Set`, `Find`, `Try`, `Handle`, `Apply`, `Remove`, `Calculate`, `Begin`, and `End`. Name arrays as plural nouns and maps as `NounByNoun`.

Use Unreal asset prefixes consistently, including examples such as `BP_`, `ABP_`, `AM_`, `BT_`, `DA_`, `DT_`, `GA_`, `GC_`, `GE_`, `IA_`, `IMC_`, `M_`, `MI_`, and `NS_`. Extend the project’s existing naming catalog only when a new asset family needs a clear, reusable prefix.

Prefer feature-based folders and GFP boundaries. Use shared folders such as `_Shared` only for genuinely shared assets. Keep framework, systems, and feature content distinct enough that ownership and dependencies are visible from the path.

For C++:

- Put the matching header first in its `.cpp` file.
- Put the generated header last among includes in a header.
- Keep public API, overrides, helpers, and private state in a predictable order.
- Use restrictive property specifiers. Prefer `EditDefaultsOnly` for class configuration and expose Blueprint read access by default rather than arbitrary write access.
- Use `UPROPERTY` for UObject references that need reflection, garbage collection, serialization, or editor exposure.
- Do not use raw `new` or `delete` for UObject lifetime. Use Unreal ownership and the appropriate pointer type for non-UObject data.
- Keep constructors safe for class defaults and object construction. Put world-dependent initialization in `BeginPlay` or an explicit initialization path.
- Keep descriptions on non-obvious Blueprint functions, pins, assets, tags, and configuration values.

### Keep module dependencies directional

Start with one runtime module when the codebase is small, then split modules when editor separation, reuse, team iteration, compile time, or ownership justifies it. Avoid tiny modules and circular dependencies.

- Expose a dependency as `PublicDependencyModuleNames` only when public headers require it. Keep implementation-only dependencies private.
- An Editor module may depend on Runtime; Runtime must not depend on Editor.
- Keep only public headers in `Public`. Keep private implementation headers and source in `Private`.
- Put shared types in a focused core module only when multiple modules genuinely need them.
- Keep optional feature dependencies behind GFP or plugin boundaries.

## Mass and large-scale simulation

Use Mass for data-oriented, high-entity-count simulation where compact fragment storage and batched processors provide a real benefit. Do not convert ordinary gameplay actors to Mass only because it is available.

- Keep fragments small, specific, and unique. Tags carry presence, not properties. Shared fragments hold data common to many entities.
- Treat finalized entity templates as immutable. Dynamic configurations need stable, unique identity.
- Choose one LOD collector, such as distance or visibility, rather than combining collectors accidentally. Add hysteresis to prevent LOD oscillation.
- Select processor phases deliberately: `PrePhysics`, `StartPhysics`, `DuringPhysics`, `EndPhysics`, `PostPhysics`, and `FrameEnd` have different ordering implications.
- Defer entity commands while processors are running. Do not commit an entity builder from inside a processing phase when the API requires deferred commands.
- Keep async spawn configuration alive for the full async operation. Use a UObject or data asset with an appropriate lifetime.
- For navigation, configure the required movement, avoidance, obstacle, orientation, steering, visualization, actor, and LOD traits. Verify which system owns actor location.
- Disable or replace off-LOD navigation behavior that teleports to a target when the design requires travel time.
- Use Visual Logger, Gameplay Debugger, and Mass debug commands to inspect entity state and processor behavior.

## Performance and diagnostics

### Profile before changing architecture

Use evidence to identify whether the bottleneck is Game Thread, Draw, GPU, Render Thread, RHI, memory, streaming, or a specific subsystem. Start with `stat unit`, `stat fps`, and `stat unitgraph`; then use targeted stats such as `stat game`, `stat gpu`, `stat scenerendering`, `stat rhi`, `stat memory`, `stat nanite`, and `stat lumen`.

Use Unreal Insights for timing, memory, task graph, or networking questions that simple stats cannot answer. Record the map, platform, scalability settings, resolution, frame rate, and reproduction steps with every comparison.

Treat benchmark claims as workload-specific. A very high FPS result usually depends on disabling expensive features or changing the content being measured; it is not a universal target for a shipped game.

### Instrument systems where decisions occur

Instrument the boundary that owns the decision:

- GAS: ability activation, commit, effect application, tag changes, cue dispatch, and EndAbility.
- Replication: authority, owner, relevancy, RPC direction, property condition, and late-join reconstruction.
- Common UI: activatable tree, layer, focus target, and input route.
- Modular Gameplay: component manager registration, init-state transitions, and injected components.
- Game Features: activation, deactivation, Experience selection, and action failure.
- Mass: entity archetype, processor phase, LOD, deferred command, and actor synchronization.
- Combat: attack window, hitbox rank/group, deduplication, damage context, i-frame tag, and hit-negated event.

Use Visual Logger for spatial and temporal events that are difficult to understand from text logs. Give logged objects stable, unique names. Use the Gameplay Debugger for in-world state and the Collision Analyzer for query problems.

### Use source and build workflows safely

Use a reproducible development loop:

1. Update the engine and project state deliberately.
2. Generate project files with the project’s supported toolchain.
3. Build the appropriate Editor target, commonly Development Editor or DebugGame Editor.
4. Run the editor and reproduce the behavior.
5. Change the smallest relevant unit and verify the result.

Hot reload is suitable for narrow body-only changes. Header, reflected function, constructor, or property changes can leave compiled Blueprint state inconsistent. If that happens, close the editor, use the project’s approved generated-data cleanup procedure, regenerate project files, and rebuild. Do not commit per-machine build configuration or engine debugging changes.

Keep engine and project versions aligned with the Lyra version used by the project. Re-save binary assets after a required engine update, and review the resulting asset changes instead of assuming they are harmless.

Choose source control from the collaboration problem. Git works well for small technical teams and branch-based source workflows. Perforce is often better when many contributors need to collaborate on binary assets and locking. In either case, define engine, project, GFP, and generated-data boundaries before the team starts branching.

## Review checklists

### Before implementation

- Is the owner, authority, and lifetime written down?
- Does the feature belong in a GFP, regular plugin, module, component, ability, effect, subsystem, or asset?
- Is the data contract separated into configuration, state, command, event, and presentation?
- Does an existing Lyra or project system already provide the needed contract?
- Which tags, inputs, assets, and module dependencies are new?
- What happens on failure, cancellation, possession change, pawn replacement, disconnect, and late join?
- What instrumentation and acceptance test will prove the feature works?

### Before merging gameplay code

- Are attributes changed through Gameplay Effects?
- Do all abilities commit and end correctly?
- Are effect handles, tasks, delegates, timers, and temporary tags cleaned up?
- Are source and target capture tags populated?
- Are snapshot semantics intentional?
- Is server validation present at the request boundary?
- Does the feature use the ASC path for replicated montages and ability state?
- Are animation windows and hitbox data authoritative enough for the design?
- Are collision channels, query flags, owner ignores, and actor deduplication verified?
- Are replicated properties and RPC reliability appropriate for frequency and loss tolerance?

### Before a network or PIE test

- Test authority, owning client, simulated client, and late-join behavior where relevant.
- Test interruption and cancellation, not only the happy path.
- Test missing assets, delayed avatar or mesh readiness, and feature activation order.
- Test respawn and possession transitions.
- Capture relevant stats, logs, Visual Logger data, or Gameplay Debugger state.
- Record engine version, map, settings, and reproduction steps.

### Before an engine or Lyra update

- Confirm the project and Lyra source versions are compatible.
- Minimize base Lyra changes and identify binary assets that need project-owned copies.
- Re-save assets in a controlled change set and inspect redirects and references.
- Rebuild every affected module and check generated headers and reflected APIs.
- Run feature activation, input, UI, GAS, replication, save/load, and Mass smoke tests.
- Do not commit local `BuildConfiguration.xml`, engine optimization changes, or generated caches.

## Source map and coverage note

The following links are the primary source map for this synthesis:

- [Quod Soler home and combat architecture index](https://www.quodsoler.com/)
- [GAS introduction](https://www.quodsoler.com/blog/introduction-to-gas-the-six-pieces-of-unreals-gameplay-ability-system), [MMC and Execution design](https://www.quodsoler.com/blog/mmcs-and-executions-in-gas-count-the-outputs-not-the-inputs), and [Gameplay Effect durations](https://www.quodsoler.com/blog/making-sense-of-gameplay-effect-durations)
- [ASC montage replication](https://www.quodsoler.com/blog/play-montages-through-the-asc-not-the-anim-instance), [animation-owned i-frames](https://www.quodsoler.com/blog/i-frames-are-animation-data-not-code), and [data-driven hitboxes](https://www.quodsoler.com/blog/hitbox-rank-and-group-sweetspots-without-scripting)
- [GAS input buffering](https://www.quodsoler.com/blog/building-souls-style-input-buffering-on-top-of-gas), [enemy dodge decisions](https://www.quodsoler.com/blog/how-enemies-decide-to-dodge), and [one-shot abilities](https://www.quodsoler.com/blog/give-ability-and-activate-once-one-shot-abilities-without-the-cleanup)
- [X157 UE5 and Lyra notes](https://x157.github.io/), including [Lyra development considerations](https://x157.github.io/UE5/LyraStarterGame/Development-Considerations), [Game Features](https://x157.github.io/UE5/GameFeatures/), [Modular Gameplay](https://x157.github.io/UE5/ModularGameplay/), [Common UI](https://x157.github.io/UE5/CommonUI/), [Enhanced Input](https://x157.github.io/UE5/EnhancedInput/), [GAS](https://x157.github.io/UE5/GameplayAbilitySystem/), [UI Extension](https://x157.github.io/UE5/UIExtension/), and [Mass](https://x157.github.io/UE5/Mass/)
- [X157 development workflow](https://x157.github.io/UE5/Dev/Workflow), [engine notes](https://x157.github.io/UE5/Engine/), [Git branch model](https://x157.github.io/Git/), [Perforce notes](https://x157.github.io/Perforce/), and [threading notes](https://x157.github.io/UE5/Threads/)
- [Unreal Directive project standards](https://unrealdirective.com/resources/project-standards/), including [folder structure](https://unrealdirective.com/resources/project-standards/folder-structure/), [naming](https://unrealdirective.com/resources/project-standards/naming-functions-variables/), [C++ standards](https://unrealdirective.com/resources/project-standards/cpp-standards/), and [module organization](https://unrealdirective.com/resources/project-standards/module-organization/)
- [Unreal Directive C++ reference](https://unrealdirective.com/resources/cpp-reference/), including [replication](https://unrealdirective.com/resources/cpp-reference/replication/), [collision](https://unrealdirective.com/resources/cpp-reference/collision/), and [GAS](https://unrealdirective.com/resources/cpp-reference/gas/)
- [Gameplay Tag setup](https://unrealdirective.com/resources/gameplay-tags/setup/), [Gameplay Tag best practices](https://unrealdirective.com/resources/gameplay-tags/best-practices/), and [Gameplay Tag patterns](https://unrealdirective.com/resources/gameplay-tags/patterns/)
- [Unreal Decoder system-selection guides](https://unrealdirective.com/resources/unreal-decoder/), [Enhanced Input guide](https://unrealdirective.com/articles/enhanced-input-what-you-need-to-know/), [Blueprint variables guide](https://unrealdirective.com/articles/blueprint-variables-what-you-need-to-know/), [Blueprint functions guide](https://unrealdirective.com/articles/blueprint-functions-what-you-need-to-know/), [Visual Logger tip](https://unrealdirective.com/tips/the-visual-logger/), and [Custom Primitive Data tip](https://unrealdirective.com/tips/custom-primitive-data/)

Unreal Directive also publishes large JavaScript-loaded catalogs, including 14,000+ console variables, 880+ engine plugins, 180+ asset-naming entries, and 100+ community resources. Those catalogs are linked lookup references rather than material that can be usefully or honestly reproduced line by line in this bible. The authored hubs and the relevant pages were synthesized into the rules above. The Quod Soler article on dashes and knockbacks with root motion sources was visible in the site index but did not load during research, so this document does not assert article-specific details from it.

When a decision depends on a version-sensitive catalog entry, open the linked reference and verify the exact engine version, module, console variable, plugin status, or API signature before implementation.
