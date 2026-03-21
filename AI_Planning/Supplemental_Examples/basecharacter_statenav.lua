
--KC: Nav state
BaseCharacter_StateNav = { name = "nav" }

function BaseCharacter_StateNav:IsAllowed( owner )
	if (owner.bMovementAllowed) then
		if (owner:CanPlayerMove() and owner.bAllowGroundNav and (owner.fStickSpeed > owner.fStickDeadzone or owner.Rollback.fForceRunTimer > 0) and owner.Rollback.fWallHitTimer < 0.5
		or (owner.actor:IsFragmentCategory(fcat_navtransition, owner.fullbodyscope_id)))then--  and owner.actor:IsOnGround()) then -- or owner.curCharVelocY > 0 and owner.actor:IsOnGround()) then
			return true;
		end
	end

	return false;
end

function BaseCharacter_StateNav:SetAnimFromFullyCommitted( owner )
	if(owner.Rollback.bPlayLandAfterCommitted == true) then
		if((owner.class == "Buccaneer" and owner.Rollback.timeInJump < 0.5) or owner.class ~= "Buccaneer") then
			owner.Rollback.iNavFrag = owner.Fragments.Jump_Land_ToRun.id
		else
			owner.Rollback.iNavFrag = owner.Fragments.Jump_Land.id
		end
		
		if(owner.Rollback.bSkipTweenAfterCommitted) then
			owner.Rollback.tweenFrames = 0
		else
			owner.Rollback.tweenFrames = 3
		end
	elseif(owner.Rollback.bPlayTransitionFromAttack == false) then
		owner.Rollback.iNavFrag = owner.Fragments.Run.id
		
		if(owner.Rollback.bSkipTweenAfterCommitted) then
			owner.Rollback.tweenFrames = 0
		else
			owner.Rollback.tweenFrames = 4
		end
	end
	
	local skipToFrame = 0
	if(owner.Rollback.bPlayTransitionFromAttack) then
		owner.Rollback.tweenFrames = 3
		--Log("owner.fAttackTransitionPerc: %f", owner.fAttackTransitionPerc)
		skipToFrame = math.floor(owner.fAttackTransitionPerc * owner.actor:QueryFragmentFramesDuration(owner.Rollback.iNavFrag))
		--Log("skipToFrame: %f", skipToFrame)
	end
	owner.fAttackTransitionPerc = 0
	owner.actor:Trans( owner.Rollback.iNavFrag, owner.Rollback.tweenFrames, skipToFrame)	
	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)
		owner:CheckForAttackInput(0.1)
		owner:CheckForLootInput(0.1)
	end
	owner:Building()
	owner:OnReturnFromCommitted()
	
end

function BaseCharacter_StateNav:SetAnimFromTurnOrLand( owner )
	owner.Rollback.iNavFrag = owner.Fragments.Run.id
	owner.actor:Trans( owner.Rollback.iNavFrag, 0, 0)	
	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)
		owner:CheckForAttackInput(0.1)
		owner:CheckForLootInput(0.1)		
	end
	owner:Building()
end

function BaseCharacter_StateNav:Enter( owner )
	owner:OnNavEnter()
	owner.Rollback.timeInJump = 0
	-- owner.Rollback.fsetWorldVelocY = 0
	-- owner.Rollback.fsetWorldVelocX = 0
	-- owner.fsetCharVelocX = 0
	-- -- owner.Rollback.fsetCharVelocY = 0
	-- clearing Z, because after a fall-to-nav state transition this wasn't getting clearing
	if(owner.Rollback.bIsBouncing == false) then
		owner.Rollback.fsetCharVelocZ = 0
	end 
	owner.Rollback.fsetMoveSpeed = 0
	owner.bAllowAirAttack = true
	if(owner.Rollback.bPlayTransitionFromAttack == false) then
		owner.Rollback.iNavFrag = owner.Fragments.Run.id
	end
	owner.fTimeInNavState = 0
	owner.Rollback.fTimeAtRunSpeed = 0
	-- owner.Rollback.fsetMoveSpeed = owner:fcurMoveSpeed
	
	-- if(owner.fpreviousNavVelocity > 0) then
		-- owner.Rollback.fsetCharVelocY = owner.fpreviousNavVelocity
		-- owner.fpreviousNavVelocity = 0
	-- end
	owner:CheckForNavStart() -- sets if we start the state with a turn anim	
	owner.Rollback.fCurJumpCount = 0
	
	local prevVelocDir = NormalizeVector(owner.prevWorldVeloc)
	local targetMoveSpeed = owner.targetSpeed
	if(prevVelocDir ~= nil and owner.fcurMoveSpeed > targetMoveSpeed and owner.fStickSpeed > owner.fStickDeadzone and LengthSqVector(prevVelocDir) > 0
	and GetAngleBetweenVectors2D(prevVelocDir, owner.actor:GetIntendedFacingDir()) * g_Rad2Deg > 60) then
		owner.Rollback.bResetSpeedOnNavStart = true
		--Log("Reset speed on nav start")
	end
	owner.Rollback.fWallHitTimer = 0
	
	if(owner.previousState ~= nil and (owner.previousState.name == "jump" or owner.previousState.name == "fall") and owner.actor:IsTagSet(owner.isLandingTagId) == false) then
		owner.actor:SetTag(owner.isLandingTagId)
	end
end

function BaseCharacter_StateNav:Exit( owner )
	-- owner.lootmodifier = 1  -- reset modifier
	--owner:StopLootAdditive()
	--owner:StartTreadmillBoost(false)
	owner:OnNavExit()
	owner.Rollback.fTreadmillCurBoost = 1.0
	owner.Rollback.fTimeAtRunSpeed = 0	
	owner.Rollback.bResetSpeedOnNavStart = false
	if(owner.actor:IsTagSet(owner.isLandingTagId)) then
		owner.actor:ClearTag(owner.isLandingTagId)
	end
end

function BaseCharacter_StateNav:Tick( owner, elapsed )
	
	local isOnGround = owner.actor:IsOnGround()
	local baseSpeed = owner:GetBaseSpeed()

	-- Attacks while on ground
	-- if(owner.bIsCarryingLoot) then
		-- owner:ThrowLootInput(0)
	-- else
		-- owner:CheckForAttackInput(0)	
	-- --	owner:NavSpecialInput(0)
	-- --	owner:AttackSpecialInput(0)
	-- end
	--owner:AttackSpecialInput2(0)
	-- owner.Rollback.bAllowLootGrab = true
	
	if(owner.Rollback.fCurJumpCount > 0) then
		owner.Rollback.fCurJumpCount = 0
	end
	
	owner.actor:ClearVelocity()
	
	if(owner.Rollback.bIsBouncing == false) then
		if(isOnGround == false and owner.fDistanceToGround < 0.5) then
			owner.Rollback.fsetCharVelocZ = -10.0
		else
			owner.Rollback.fsetCharVelocZ = 0.0
		end
	end
		
	owner:SetIntendedFacingAndInputDiff()
	local intendedFacing = owner.actor:GetIntendedFacingDir()
	
	--owner:UpdateWallCollisionTimer(false, 0.8, 0.2, true)
					
	if(not owner.actor:IsFragmentPlaying( owner.Rollback.iNavFrag, owner.fullbodyscope_id)) then
		if(not (owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id)
		or owner.actor:IsFragmentCategory(fcat_navtransition, owner.fullbodyscope_id))) then
			if(isOnGround) then
				owner.actor:Trans( owner.Rollback.iNavFrag, owner.Rollback.tweenFrames, 0)
				owner.Rollback.tweenFrames = 0
			end
		end
	end	
	
	-- Restart trans with a blend if we're suddenly going to move more than 90 degrees away from our existing velocity direction
	if(owner.Rollback.iNavFrag == owner.Fragments.Run.id and owner.fcurMoveSpeed > 0 and owner.fStickSpeed > owner.fStickDeadzone) then
		if(vecDot(owner.actualWorldVeloc, intendedFacing) < 0) then
			owner.actor:Trans(owner.Rollback.iNavFrag, 17, 0)
		end
	end
					
	-- if(owner.Rollback.fForceRunTimer == 0 and (owner.fStickSpeed < owner.fStickDeadzone or owner.fAbsInputAngleDiff > 135.0)) then
		-- if(not (owner.actor:IsFragmentCategory(fcat_turn, owner.fullbodyscope_id) 
		-- or owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id)
		-- or owner.actor:IsFragmentCategory(fcat_navtransition, owner.fullbodyscope_id))) then
			-- if(owner.fcurMoveSpeed >= owner:GetBaseSpeed() * 0.9) then
				-- owner.Rollback.iNavFrag = owner.Fragments.Run_ToIdle.id
				-- owner.Rollback.tweenFrames = 4
				-- vecSkidDir = owner:GetDirectionVector()
				-- owner.Rollback.bSkipTransition = true
				-- owner:StartTreadmillBoost(false)
				-- owner.Rollback.fTreadmillCurBoost = 1.0
			-- end
		-- end
	-- end
		
	-- if(owner.Rollback.fForceRunTimer == 0 and owner.fcurMoveSpeed >= owner:GetBaseSpeed() * 0.9 and owner.fAbsInputAngleDiff > 135.0 and owner.Rollback.iNavFrag ~= owner.Fragments.Run_ToIdle.id
		-- and not (owner.actor:IsFragmentCategory(fcat_turn, owner.fullbodyscope_id) 
		-- or owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id)
		-- or owner.actor:IsFragmentCategory(fcat_navtransition, owner.fullbodyscope_id))) then
		-- owner.Rollback.iNavFrag = owner.Fragments.Run_ToIdle.id
		-- owner.actor:Trans( owner.Rollback.iNavFrag, 4, 0)
	-- end
	
	-- local skidActive = false
	
	-- if(owner.Rollback.iNavFrag == owner.Fragments.Run_ToIdle.id) then
		-- --skidActive = true
	-- end
		
	-- if(owner.fStickSpeed < 0.9 or owner.fAbsInputAngleDiff > 135.0) then
		-- owner.Rollback.fTreadmillCurBoost = 1.0
	-- end
			
	local targetMoveSpeed = owner.stats:GetStat(eS_moveSpeed)
	if(owner.Rollback.fForceRunTimer == 0 and owner.fStickSpeed < 0.7) then
		
		if(owner.fStickSpeed < owner.fStickDeadzone) then
			targetMoveSpeed = 0
		else
			targetMoveSpeed = owner.targetSpeed	--(owner.fWalkSpeed + owner:GetSpeedBoost()) * owner:GetCurTurboMod()
		end
	else
		if(owner.bIsCarryingLoot) then
			targetMoveSpeed = owner.targetSpeed	--(owner:GetEncumberedSpeed() + owner:GetSpeedBoost()) * owner:GetCurTurboMod()
		else
			targetMoveSpeed = owner.targetSpeed	--(owner.stats:GetStat(eS_moveSpeed) + owner:GetSpeedBoost()) * owner:GetCurTurboMod()
		end
	end
	
	-- if(skidActive) then
		-- targetMoveSpeed = 0
	-- end
	
	if(owner.fStickSpeed > owner.fStickDeadzone) then
		targetMoveSpeed = math.max(targetMoveSpeed, 2)
	else
		targetMoveSpeed = 0
	end
		
	if(targetMoveSpeed == 0) then
		local lerpSpeed = 10
		
		-- if(skidActive) then
			-- lerpSpeed = 15
		-- else
			lerpSpeed = 50
		--end
		
		owner.Rollback.fsetMoveSpeed = VariableInterpolate(owner.fcurMoveSpeed, targetMoveSpeed, elapsed, lerpSpeed);

		--velocity.y = owner.Rollback.fsetCharVelocY
	else	
		local interpSpeed = 4.0
		if(owner.Rollback.bTreadmillBoostActive) then
			interpSpeed = 1.5
		end
		if(owner.Rollback.bSkipSpeedInterpolation) then
			owner.Rollback.fsetMoveSpeed = targetMoveSpeed
			owner.Rollback.bSkipSpeedInterpolation = false
		else
			owner.Rollback.fsetMoveSpeed = VariableInterpolate(owner.fcurMoveSpeed, targetMoveSpeed, elapsed, interpSpeed)
		end
		
		if(owner.Rollback.bResetSpeedOnNavStart) then
			owner.Rollback.bResetSpeedOnNavStart = false
			owner.Rollback.fsetMoveSpeed = targetMoveSpeed
		-- elseif(owner.fStickSpeed > owner.fStickDeadzone and owner.Rollback.fsetMoveSpeed < owner.fWalkSpeed) then
			-- owner.Rollback.fsetMoveSpeed = owner.fWalkSpeed
		end	
				
		local speedRatio = clamp(targetMoveSpeed/baseSpeed, 0.7, 1.5)
		owner.actor:SetFragmentSpeed(owner.Fragments.Run.id, speedRatio);
	end

	--Log( string.format( "y velocity %f", velocity.y ) );
	
----TEST FOR SPEED CHANGE WHEN GOING UP OR DOWN INCLINES------	
	-- if(owner.bIsCarryingLoot) then
	-- --	owner.inclinemodifier = .5	
		-- owner.lootmodifier = .5			
	-- else
	-- --	owner.inclinemodifier = 1		
		-- owner.lootmodifier = 1				
	-- end
--	velocity.y = velocity.y*owner.inclinemodifier
	-- owner.Rollback.fsetMoveSpeed = owner.Rollback.fsetMoveSpeed * owner.lootmodifier
	
----END OF TEST FOR SPEED CHANGE WHEN GOING UP OR DOWN INCLINES------
		
	if(owner.Rollback.allowRotation) then
		
		local turnSpeedMod = 1.0
		
		-- if(owner.fAbsInputAngleDiff > 135.0) then
			-- if(owner.Rollback.fsetMoveSpeed < owner.stats:GetStat(eS_moveSpeed)) then
				-- turnSpeedMod = 4.0
			-- end
		-- end
		
		if(owner.Rollback.fTreadmillBoostTimer > 0) then
			turnSpeedMod = 1.0 - (owner.Rollback.fTreadmillBoostTimer / owner.fTreadmillBoostDuration)
		elseif(owner.Rollback.fRampBoostTimer > 0) then
			turnSpeedMod = 1.0 - (owner.Rollback.fRampBoostTimer / owner.fRampBoostDuration)
		elseif(owner.actor:IsFragmentCategory(fcat_fastturn, owner.fullbodyscope_id)) then
			turnSpeedMod = 2.0
		end
		
		-- local forceLeftTurn = false
		-- local forceRightTurn = false
		-- if(owner.actor:IsFragmentPlaying( owner.Fragments.Run_Turn180_ToRun.id, owner.fullbodyscope_id)) then
			-- -- Only force right turn if we have a large angle delta to our desired facing (when we're close the system will continue to select "turn right" if we're already going that way).
			-- -- If we continue to force a right turn when we're close, then we might pass our intended direction (because of slight user input adjustments on the stick)
			-- -- in which case we'll have to then keep turning right to get around again to our desired facing direction.
			-- local desiredFacingAngleDelta = math.acos(vecDot(intendedFacing, owner:GetDirectionVector())) * g_Rad2Deg

			-- if (desiredFacingAngleDelta > 90) then
				-- if(owner.bForce180Left == true) then
					-- forceLeftTurn = true
				-- else
					-- forceRightTurn = true
				-- end
			-- end

			-- turnSpeedMod = 1.0
		-- elseif(owner.Rollback.iNavFrag == owner.Fragments.Run_ToIdle.id) then
			-- turnSpeedMod = 0
		-- end
		
		--owner:RotateTowardsIntendedFacing(owner.turnSpeed * turnSpeedMod, forceLeftTurn, forceRightTurn)
		owner:RotateTowardsIntendedFacing(10800 * turnSpeedMod, false, false)
	end

	owner.fDontAllowTransitionTimer = owner.fDontAllowTransitionTimer - elapsed
	
	owner.fTimeInNavState = owner.fTimeInNavState + elapsed
	
	if(targetMoveSpeed >= baseSpeed and owner.fStickSpeed >= 0.9) then
		owner.Rollback.fTimeAtRunSpeed = owner.Rollback.fTimeAtRunSpeed + elapsed
	else
		owner.Rollback.fTimeAtRunSpeed = 0
	end
	--owner.curCharVelocZ = VariableInterpolate(owner.curCharVelocZ, -10.0, elapsed, 5.0)
	--velocity.z = owner.curCharVelocZ
	
	local dirVector = intendedFacing
	
	-- if(skidActive) then
		-- dirVector = vecSkidDir
	-- else
	-- if(owner.actor:IsFragmentPlaying( owner.Fragments.Run_ToIdle.id, owner.fullbodyscope_id )) then
		-- dirVector = NormalizeVector(owner.actualWorldVeloc)
	-- else
	if(owner.Rollback.allowRotation == false or owner.Rollback.fForceRunTimer > 0) then
		dirVector = owner:GetDirectionVector()
	end
		
	local targetSpeedX = dirVector.x * owner.Rollback.fsetMoveSpeed
	local targetSpeedY = dirVector.y * owner.Rollback.fsetMoveSpeed
	
	local velocity = {x=targetSpeedX,y=targetSpeedY,z=owner.Rollback.fsetCharVelocZ}
	
	--QUICK SPEED CHECK START--
	local desiredMoveSpeed = owner.Rollback.fsetMoveSpeed
	local actualMoveSpeed = owner.fcurMoveSpeed
	--QUICK SPEED CHECK END--
	
	if(owner.actor:GetHitStop() <= 0 and owner.actor:IsHoming() == false) then
		owner.actor:SetAdditionalWorldVelocity(velocity)
	end

	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)	
		owner:CheckForAttackInput(0.1)
		owner:CheckForLootInput(0.1)
		owner:CheckPrecisionModeInput()
	end
	owner:UpdateRelicGrabStatus()
	owner:Building()
end

function BaseCharacter_StateNav:DuringCommitted( owner, elapsed )
	-- owner.Rollback.bAllowLootGrab = false
	owner.fDontAllowTransitionTimer = owner.fDontAllowTransitionTimer - elapsed
end

