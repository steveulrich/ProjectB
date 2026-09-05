# Passive gold replication regression

September 5, 2026. Status: source fix compiled successfully; multiplayer verification pending.

## Evidence and cause

Saved/Logs/ProjectB.log records `FActiveGameplayEffect::PostReplicatedAdd Received ReplicatedGameplayEffect with no UGameplayEffect def` on August 30 and 31 multiplayer sessions (for example lines 7081, 7124, and 7179 in the inspected log). The current passive-income code created an infinite UGameplayEffect in the transient package on the server. Clients cannot resolve that runtime-only definition. This is a source-confirmed defect consistent with those errors; a fresh run is required to prove that it accounts for the observed errors.

The replacement uses UBwayGameplayEffect_PassiveGold, a native shared definition, with the income magnitude supplied on each outgoing spec. The RoundManagement component applies it only on authority to each PlayerState ASC. GAS replicates the active effect through the existing ASC policy, and CurrentGold retains its existing replicated attribute and RepNotify. No RPC, relevancy, dormancy, update-frequency, or prediction changes are required. The existing gold clamps remain in BwayGoldAttributeSet.

The one-second period, no immediate application tick, and existing round-end handle removal remain unchanged. Round start removes old income handles before applying new ones. The nearby transient instant gold delta does not create a persistent active effect and is outside this defect.

## Required verification

1. Rebuild LyraEditor for Win64 Development and restart the editor; this change introduces a reflected class.
2. Start DevMap as a listen server with at least one remote client and bots filling to eight. Record the configured income rate.
3. Inspect the owning remote client's active effect: its definition must resolve to BwayGameplayEffect_PassiveGold. Confirm no missing-definition errors on any client.
4. During Playing, compare host/server and owning-client CurrentGold: one configured increment per second, subject to MaxGold. There must be no immediate extra tick when income starts.
5. End the round. Gold must persist, periodic income must stop, and the next round must resume at one increment per second without stacking old effects.
6. Finish the match and enter a new match. Confirm the specified match reset and no leftover income handles.
7. Repeat with packet latency enabled and record the emulation settings. Client display may arrive later, but authoritative totals must converge without duplicated income.
8. Repeat the same checks on Dorado when map parity is ready.

Compile passed; PIE, packaged-build, and latency checks remain pending. These checks do not establish unrelated hero, relic, or complete slice gates.

## Prevention

Duration and infinite Gameplay Effects need a client-resolvable asset or native class definition. Supply per-application values through the spec (for example SetByCaller); never mutate a shared definition or create a transient server-only definition for an active replicated effect.

## Build verification update

September 5, 2026: LyraEditor Win64 Development build succeeded (exit 0), using UE 5.8.1. Evidence: Saved/Logs/codex-tier1-20260905-retry.log. The initial build compiled both gameplay fixes but failed in existing bridge JSON key conversions; those conversions were fixed and the subsequent build passed. Editor launched after build success. Multiplayer acceptance checks remain pending.
