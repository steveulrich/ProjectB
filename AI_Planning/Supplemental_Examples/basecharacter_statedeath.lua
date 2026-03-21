BaseCharacter_StateDeath = { name = "death" }

function BaseCharacter_StateDeath:IsAllowed( owner )

	if( (CryAction.IsServer() and owner.actor:GetHealth() <= 0.0) or
		(CryAction.IsClient() and not CryAction.IsServer() and owner.synched.CurrenServerStateName == BaseCharacter_StateDeath.name) ) then
		 
			return true
	end
	
	return false

end

function BaseCharacter_StateDeath:SetAnimFromFullyCommitted( owner )	
	if(not owner.actor:IsFragmentPlaying( owner.Rollback.iDeathFrag, owner.fullbodyscope_id)) then
		owner.actor:Trans( owner.Rollback.iDeathFrag, 0, 0)	
	end	
	owner:OnReturnFromCommitted()
end

function BaseCharacter_StateDeath:SetAnimFromTurnOrLand( owner )
end

function BaseCharacter_StateDeath:Enter( owner )
    Log("Enter Death")
	owner.Rollback.bStopMoving = false

	-- detach loot if any
	--owner:ThrowAllAvailableLoot();
	owner.Rollback.bOnlyOnce = true
	owner:StopRampBoost()
	
	local deathRespawnTime = 0;
	local killedBy = owner.actor:GetOpponent()
	owner:FumbleRelicAwayFromAttacker(killedBy)

	-- Set respawn delay
	if (AllowPlayerRespawn()) then
		deathRespawnTime = GetPlayerRespawnTime(owner)
		owner:SetDeathStateTimer(deathRespawnTime)
	else
		--owner.fJumpGravity = 25
		deathRespawnTime = 7;
		owner:SetDeathStateTimer(deathRespawnTime)
	end

	owner.fLastDeathRespawnTimeDuration = deathRespawnTime;

	--owner:DeathStart()
	-- owner.Rollback.bAllowLootGrab = false
	owner.Rollback.allowRotation = false
	owner:SetIsAlive(false)
	owner:ClearSelectedBuildableFromServer()
	owner.actor:ShowBuildableGhost(false)
	if(owner.actor:IsCamouflaged()) then
		owner:ResetCamouflage()
	end
	owner:UpdateFallAudio()
	--[[ Remove this for now since we're not using it.
	local shouldLoseItems = System.GetCVar("rly_loseItemOnDeath");
	local loseOneItem = System.GetCVar("rly_loseOneOnDeath");
	if(shouldLoseItems == nil or shouldLoseItems == 1) then
		owner.actor:ClearEquipment(loseOneItem == nil or loseOneItem == 0);
	end
	]]--

	local localDeathSFX = owner.deathLocalSFX
	local remoteDeathSFX = owner.deathOthersSFX
	if (Game.GetNumPlayersAlive(owner.team) == 0) then
		localDeathSFX = owner.deathSlowMoSFX
		remoteDeathSFX = owner.deathSlowMoSFX
	elseif(owner.actor:GetDeathType() == DeathType_Water) then
		localDeathSFX = owner.sfxDeathWaterLocal
		remoteDeathSFX = owner.sfxDeathWaterOthers
	end

	-- For AI, we want to make sure we hear the remote SFX when they die (without this check, the game will think they are the local client and play the local SFX in standalone)
	if (owner.isAI == true) then
		localDeathSFX = remoteDeathSFX
	end
	Game.PlayAudioOnPlayer(localDeathSFX, remoteDeathSFX, owner.id)

	-- TODO: Add in assists (currently using '0')
	-- Coin value = KDA ratio * 100 (only multiply by 10 below since each coin is already worth 10)

	owner:ClearNonPermanentBuffs()
	owner:ResetAttackTimers()
	
	--owner:DropDeathCoins()
	--owner:EnablePhysics(false)
	owner.actor:EnableCharacterToCharacterCollision(false)
	owner:ShowCharacterPassiveFX(false)

	owner:SetAllowedToPurchaseEquipment(true)
	
	-- if(Game.GetNumPlayersAlive(owner.team) == 0) then
		-- owner.Rollback.iDeathFrag = owner.Fragments.Death_Special.id
	-- else
		owner.Rollback.iDeathFrag = owner.Fragments.Death.id
	--end
	
	owner:StopUpperbodyScope()
	owner:StopHitshakeAdditiveScope()
	owner:ResetAbilitySelection()

	-- Heroic Phoenix Ashes: +MaxHP per death within a round (Phoenix Ashes is a permanent buff that will be cleared at the start of the next round)
	local equip = nil
	for i=1, owner.actor:GetNumEquips() do
		equip = owner.actor:GetEquipment(i-1)
		if(equip:GetCurrentLevel() == eml_hero and equip.name == "Reaper") then
			owner.stats:AddBuff(2793964437)
		end
	end
end

function BaseCharacter_StateDeath:Exit( owner )
    Log("Exit Death")
	owner:DoSpawn(true)
	owner:ResetVariables()
   	owner.actor:ToggleHitCapsule("center", true)  -- putting this here because i don't know of a better place to put this on init -- DOH 1/14/15
	owner.fDeathStateEnterTimer = 0
	owner.Rollback.bOnlyOnce = false
	owner.actor:ResetKillStreak()
	owner.actor:DisableSpectateCamera()
end

function BaseCharacter_StateDeath:Tick( owner, elapsed )		
		-- Spectate camera controls
        if(owner.wasCycleRightPressed and owner.wasCycleRightPressed > 0) then
            owner.wasCycleRightPressed = owner.wasCycleRightPressed - elapsed
        end
        if(owner.wasCycleLeftPressed and owner.wasCycleLeftPressed > 0) then
            owner.wasCycleLeftPressed = owner.wasCycleLeftPressed - elapsed
        end
		if (owner:Pressed(2824970682, 0.1) and (not owner.wasCycleRightPressed or owner.wasCycleRightPressed <= 0)) then
            if(owner.wasCycleRightPressed) then Log("owner.wasCycleRightPressed" .. owner.wasCycleRightPressed) end
            owner.wasCycleRightPressed = 0.2
			owner.actor:SpectateNextAlivePlayer()
		else
            if (owner:Pressed(3184667449, 0.1) and (not owner.wasCycleLeftPressed or owner.wasCycleLeftPressed <= 0)) then
            if(owner.wasCycleLeftPressed) then Log("owner.wasCycleLeftPressed" .. owner.wasCycleLeftPressed) end
                owner.wasCycleLeftPressed = 0.2
			    owner.actor:SpectatePrevAlivePlayer()
            end
		end

		-- Respawn logic
		if (AllowPlayerRespawn() and CryAction.IsServer()) then
			if (owner:GetDeathStateTimer() > 0 and g_gameRules:IsInGame()) then
				owner:SetDeathStateTimer(owner:GetDeathStateTimer() - elapsed)
				if (owner:GetDeathStateTimer() <= 0) then
					MetricsReport.PlayerRespawn(owner.id, owner.fLastDeathRespawnTimeDuration);
					owner.actor:RestoreHealth();

					if (g_gameRules ~= nil and g_gameRules.Event_OnPlayerRespawn ~= nil) then
						g_gameRules:Event_OnPlayerRespawn(owner)
					end
				end
			end
		end

	--	if(owner.Rollback.bOnlyOnce == false) then 
		if(owner.Rollback.bStopMoving) then 
		--	owner.actor:ClearAdditionalVelocity()
			owner.actor:ClearVelocity()			
		-- else
			-- owner:ReactionTick()		
		end
	--	else
	--		owner.actor:SetAdditionalVelocity({x=0,y=0,z=0});
	--	end
		if(not owner.actor:IsFragmentPlaying( owner.Rollback.iDeathFrag, owner.fullbodyscope_id)) then
			if( not owner.actor:IsFragmentCategory(fcat_death, owner.fullbodyscope_id)) then 
				owner.actor:Trans( owner.Rollback.iDeathFrag, owner.Rollback.tweenFrames, 0)
			end
		end		

	owner.actor:SetActiveEnvironment()
end

function BaseCharacter_StateDeath:DuringCommitted( owner, elapsed )

end


