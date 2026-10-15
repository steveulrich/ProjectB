--KC: Idle state
BaseCharacter_StateIdle = { name = "idle" }

function BaseCharacter_StateIdle:IsAllowed( owner )
	return true
end

function BaseCharacter_StateIdle:Enter( owner )
	owner:OnIdleEnter()
	owner.Rollback.timeInJump = 0
	owner.Rollback.fsetCharVelocY = 0
	if(owner.Rollback.bIsBouncing == false) then
		owner.Rollback.fsetCharVelocZ = 0
	end
	owner.Rollback.fsetWorldVelocY = 0
	owner.Rollback.fsetWorldVelocX = 0
	owner.Rollback.fOffGroundTimer = 0
	owner.bAllowAirAttack = true
	--owner.actor:ClearVelocity()
	owner.fAccumulatedRunAcceleration = 0
	owner:CheckForIdleStart()
	owner.Rollback.fCurJumpCount = 0
	owner.Rollback.bHitByShockwave = false
	owner.Rollback.fFidgetCounter = 1
	if(owner.Rollback.bTreadmillBoostActive) then
		owner:StartTreadmillBoost(false)
	end
	
	if(owner.previousState ~= nil and (owner.previousState.name == "jump" or owner.previousState.name == "fall") and owner.actor:IsTagSet(owner.isLandingTagId) == false) then
		owner.actor:SetTag(owner.isLandingTagId)
	end
end

function BaseCharacter_StateIdle:SetAnimFromFullyCommitted( owner )
	if(owner.Rollback.bPlayLandAfterCommitted == true) then
		owner.Rollback.iIdleFrag = owner.Fragments.Jump_Land.id
		owner.Rollback.tweenFrames = 3
	else
		owner.Rollback.iIdleFrag = owner.Fragments.Idle.id
		owner.Rollback.tweenFrames = 8
	end
	owner.actor:Trans( owner.Rollback.iIdleFrag, owner.Rollback.tweenFrames, 0)
	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)
		owner:CheckForAttackInput(0.1)	
		owner:CheckForLootInput(0.1)
	end
	owner:Building()
	owner:OnReturnFromCommitted()
end

function BaseCharacter_StateIdle:SetAnimFromTurnOrLand( owner )
	owner.Rollback.iIdleFrag = owner.Fragments.Idle.id
	owner.Rollback.tweenFrames = 8
	owner.actor:Trans( owner.Rollback.iIdleFrag, owner.Rollback.tweenFrames, 0)	
	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)	
		owner:CheckForAttackInput(0.1)	
		owner:CheckForLootInput(0.1)
	end
	owner:Building()
end

function BaseCharacter_StateIdle:Exit( owner )
	owner:OnIdleExit()
	owner:ResetFidgetCounter()
	--owner:StopLootAdditive()
	if(owner.actor:IsTagSet(owner.isLandingTagId)) then
		owner.actor:ClearTag(owner.isLandingTagId)
	end
end

function BaseCharacter_StateIdle:Tick( owner, elapsed )

	if(not owner.actor:IsFragmentPlaying( owner.Rollback.iIdleFrag, owner.fullbodyscope_id)) then
		if(not (owner.actor:IsFragmentCategory(fcat_turn, owner.fullbodyscope_id))
		and not (owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id))
		and not (owner.actor:IsFragmentCategory(fcat_idletransition, owner.fullbodyscope_id))) then
			owner.actor:Trans( owner.Rollback.iIdleFrag, owner.Rollback.tweenFrames, 0)	
			owner.Rollback.tweenFrames = 0
		end
	end
	
	local skipTurns = owner.class == "MorganLeFay" or owner.class == "XuFu"
	
	local camDir = owner.actor:GetCameraDir()
	camDir.z = 0
	local intendedFacing = camDir
	if(not System.IsMultiplayer() and owner.isAI) then
		intendedFacing = owner.actor:GetIntendedFacingDir()
	end

	local cameraAngleDiff = owner.actor:GetAngleDiffFromFacing(intendedFacing)	--GetAngleBetweenVectors(intendedFacing, owner:GetDirectionVector()) * g_Rad2Deg
	--Log("cameraAngleDiff: %f", cameraAngleDiff)
	if(owner.bMovementAllowed and skipTurns == false) then
		if(cameraAngleDiff > 45) then
			owner.Rollback.iIdleFrag = owner.Fragments.Idle_TurnL_90.id
		elseif(cameraAngleDiff < -45) then
			owner.Rollback.iIdleFrag = owner.Fragments.Idle_TurnR_90.id	
		end
		owner.Rollback.tweenFrames = 0
	end
	
	-- if(IsRelicBattle() and g_gameRules.IsInRoundResults and g_gameRules:IsInRoundResults() and g_gameRules:GetRoundEndWinningTeamIdx() == owner.team and Game.GetConfigBool("Gameplay.EnableEndzoneCelebrations")) then
	-- 	owner.actor:Trans(owner.Fragments.EndZone_Celebration_01.id, 4, 0)
	-- 	--Log("EndZone_Celebration_01")
	-- end
	
	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)	
		owner:CheckForAttackInput(0.1)	
		owner:CheckForLootInput(0.1)
		owner:CheckPrecisionModeInput()
		owner:CheckForTaunt(0)
	end
	owner:UpdateRelicGrabStatus()
	owner:Building()

	owner:UpdateWallCollisionTimer(false, 0.8, 0.1)
	
	if(owner.Rollback.fCurJumpCount > 0) then
		owner.Rollback.fCurJumpCount = 0
	end

	-- owner.Rollback.bAllowLootGrab = true
	
	--owner:AttackSpecialInput2(0)
	--owner:SetIntendedFacingAndInputDiff()
	if(owner.Rollback.allowRotation and owner.bMovementAllowed) then
		local turnSpeed = 400
		local absCamAngleDiff = math.abs(cameraAngleDiff)
		if(absCamAngleDiff > 150 ) then
			turnSpeed = 3200
		elseif(absCamAngleDiff > 120) then
			turnSpeed = 2400
		elseif(absCamAngleDiff > 90) then
			turnSpeed = 1600
		elseif(absCamAngleDiff > 60) then
			turnSpeed = 800
		end
				
		if(owner.actor:IsFragmentCategory(fcat_attack, owner.upperbodyscope_id) == false and (skipTurns or owner.actor:IsFragmentCategory(fcat_turn, owner.fullbodyscope_id) or owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id)
		or owner.actor:IsFragmentCategory(fcat_idletransition, owner.fullbodyscope_id))) then
			owner:RotateTowardsIntendedFacing(turnSpeed)
			--Log("Idle Rotate")
		end
	end
	owner.fDontAllowTransitionTimer = owner.fDontAllowTransitionTimer - elapsed

	owner.actor:SetActiveEnvironment()
end

function BaseCharacter_StateIdle:DuringCommitted( owner, elapsed )
	-- owner.Rollback.bAllowLootGrab = false
end

