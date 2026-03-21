BaseCharacter_StateSlide =
{
	name = "slide",
	bUsingSlideGroundAngles = false,
}

function BaseCharacter_StateSlide:IsAllowed( owner )
	if (owner.bMovementAllowed) then
		if(owner:CanPlayerMove() and owner.Rollback.bRampBoostActive == false and owner.Rollback.fSlideCooldownTimer <= 0 and owner:Held_Slide()) then
			return true
		end
	end
	return false;
end

function BaseCharacter_StateSlide:Enter( owner )
	--Log("Enter Slide")
	owner:OnSlideEnter()
	if(owner.actor:IsFragmentCategory(fcat_dodge, owner.fullbodyscope_id) == false) then
		owner:InheritVelocity(true)
	else
		owner.Rollback.fsetWorldVelocX = owner:GetDirectionVector().x * owner.stats:GetStat(eS_slideSpeed)
		owner.Rollback.fsetWorldVelocY = owner:GetDirectionVector().y * owner.stats:GetStat(eS_slideSpeed)
		owner.Rollback.fsetCharVelocZ = owner.actualCharVeloc.z
		owner.Rollback.fsetWorldVelocZ = owner.actualWorldVeloc.z
		--Log("Enter slide from dodge")
	end

	owner:CheckForSlideStart()

	owner:ResetSlideVelocity()
	self:SetSlideGroundAngles(owner)
	owner.Rollback.bIsSliding = true
	if(owner.actor:IsOnGround()) then
		owner.effects:StartEffect(1910235783)
	end

	owner.Rollback.vSlideStartPos = owner:GetPos()
	MetricsReport.PlayerSlideStart(owner.id)
end

function BaseCharacter_StateSlide:SetAnimFromFullyCommitted( owner )
	if(owner.actor:IsOnGround()) then
		owner.Rollback.iSlideFrag = owner.Fragments.Idle_ToSlide.id
		owner.Rollback.tweenFrames = 0
	else
		owner.Rollback.iSlideFrag = owner.Fragments.Jump_ToSlide.id
		owner.Rollback.tweenFrames = 0
	end
	owner.actor:Trans( owner.Rollback.iSlideFrag, owner.Rollback.tweenFrames, 0)
	if(owner.bPerformInputCheck) then
		owner:CheckForAttackInput(0.1)	
		owner:CheckForLootInput(0.1)
	end
	
	if(owner.actor:IsFragmentCategory(fcat_dodge, owner.fullbodyscope_id) == false) then
		owner:InheritVelocity(true)
	else
		owner.Rollback.fsetWorldVelocX = owner:GetDirectionVector().x * owner.stats:GetStat(eS_slideSpeed)
		owner.Rollback.fsetWorldVelocY = owner:GetDirectionVector().y * owner.stats:GetStat(eS_slideSpeed)
		owner.Rollback.fsetCharVelocZ = owner.actualCharVeloc.z
		owner.Rollback.fsetWorldVelocZ = owner.actualWorldVeloc.z
		--Log("Enter slide from dodge")
	end
	--Log("SetAnimFromFullyCommitted")
	owner:ResetSlideVelocity()
	owner:OnReturnFromCommitted()
end 

function BaseCharacter_StateSlide:SetAnimFromTurnOrLand( owner )
	--Log("SetAnimFromTurnOrLand")
	owner.Rollback.iSlideFrag = owner.Fragments.Slide.id
	owner.Rollback.tweenFrames = 4
	owner.actor:Trans( owner.Rollback.iSlideFrag, owner.Rollback.tweenFrames, 0)
	if(owner.bPerformInputCheck) then
		owner:CheckForAttackInput(0.1)	
		owner:CheckForLootInput(0.1)	
	end
	-- owner:InheritVelocity(true)
	-- owner:ResetSlideVelocity()
end

function BaseCharacter_StateSlide:SetStandardGroundAngles(owner)
	if (self.bUsingSlideGroundAngles == false) then
		owner.actor:SetMaxClimbAngle(owner.PhysicsParams.PlayerDynamics.maxClimbAngle)
	    owner.actor:SetMinFallAngle(owner.PhysicsParams.PlayerDynamics.minFallAngle)
	    owner.actor:SetMinSlideAngle(owner.PhysicsParams.PlayerDynamics.minSlideAngle)
	    self.bUsingSlideGroundAngles = true
	end
end

function BaseCharacter_StateSlide:SetSlideGroundAngles(owner)
	if (self.bUsingSlideGroundAngles == true) then
		owner.actor:SetMaxClimbAngle(owner.PhysicsParams.PlayerDynamics.maxClimbAngleDuringSlide)
	    owner.actor:SetMinFallAngle(owner.PhysicsParams.PlayerDynamics.minFallAngleDuringSlide)
	    owner.actor:SetMinSlideAngle(owner.PhysicsParams.PlayerDynamics.minSlideAngleDuringSlide)
	    self.bUsingSlideGroundAngles = false
	end
end

function BaseCharacter_StateSlide:Exit( owner )
	owner:OnSlideExit()
	
	if(owner.fcurMoveSpeed < owner.stats:GetStat(eS_moveSpeed) * owner.Rollback.fTreadmillCurBoost) then
		owner:StartTreadmillBoost(false)
		owner.Rollback.fTreadmillCurBoost = 1.0
	end

	owner.bAllowJumpBufferIncrement = true
	owner.Rollback.allowRotation = true

	if( owner.bSlideSoundPlaying == true ) then
		owner.effects:StopEffect(949942369)
		owner.bSlideSoundPlaying = false
	end

	self:SetStandardGroundAngles(owner)
	owner.Rollback.bIsSliding = false
	owner.effects:StopEffect(2229931659)
	owner.bSlideDustVFXIsPlaying = false

	MetricsReport.PlayerSlideEnd(owner.id, owner.Rollback.vSlideStartPos)
	owner.Rollback.vSlideStartPos = nil
end

function BaseCharacter_StateSlide:Tick( owner, elapsed )
	
	
	
	-- Control values
	local maxSpeed = 3.5 * owner.stats:GetStat(eS_slideSpeed)
	local minSlopeAngle = 1
	local slopeAcceleration = 9 * owner.stats:GetStat(eS_slideSpeed)
	if(owner.bIsCarryingLoot) then	
		maxSpeed = maxSpeed*.7
		slopeAcceleration = slopeAcceleration*.9
	end	
	local steerSpeed = 2.0
	local rotationSpeed = 10800	--300.0 	-- Has nothing to do with slide velocity, just controls how fast character faces slide direction

	if(owner.actor:IsOnGround() == true and owner.bSlideSoundPlaying == false) then
		owner.effects:StartEffect(949942369)
		owner.bSlideSoundPlaying = true
	elseif(owner.actor:IsOnGround() == false and owner.bSlideSoundPlaying == true) then
		owner.effects:StopEffect(949942369)
		owner.bSlideSoundPlaying = false
	end

	if(owner.bPerformInputCheck) then
		owner:BuildableInput(0.1)
		owner:CheckForAttackInput(0.1)
	end
	owner:UpdateRelicGrabStatus()
	owner:Building()
	owner:UpdateWallCollisionTimer(true, 0.5, 0.2)

	if(owner.Rollback.fBuildTimer <= 0 and owner.actor:IsFragmentPlaying( owner.Rollback.iSlideFrag, owner.fullbodyscope_id) == false) then
		if(owner.actor:IsFragmentCategory(fcat_slidetransition, owner.fullbodyscope_id) == false) then
			owner.actor:Trans( owner.Rollback.iSlideFrag, owner.Rollback.tweenFrames, 0)	
			owner.Rollback.tweenFrames = 0
		end	
	end
	
	if(owner.fStickSpeed > owner.fStickDeadzone) then
		owner:SetIntendedFacingAndInputDiff()
	end
	
	owner.fDontAllowTransitionTimer = owner.fDontAllowTransitionTimer - elapsed
	
	if(owner:Pressed_Jump(0.1)) then
		if(owner.Rollback.fJumpCooldownTimer <= 0 and (owner.fDistanceToGround < 0.2 or owner.Rollback.fOffGroundTimer < 0.1)) then
			owner.Rollback.fSlideCooldownTimer = 0.2
			owner.Rollback.bIsSliding = false
			owner.Rollback.iJumpFrag = owner.Fragments.Slide_ToJump.id
			owner.Rollback.tweenFrames = 0
			owner.actor:Trans( owner.Rollback.iJumpFrag, owner.Rollback.tweenFrames, 0)
			owner.Rollback.bleavingground = true
			owner.Rollback.fSlideJumpTimer = 1.0
		end
	end
		
	if (owner.actor:IsOnGround()) then
		self:SetSlideGroundAngles(owner)

		owner.Rollback.allowRotation = true
		if(owner.Rollback.bIsFalling == true) then
			owner.Rollback.iSlideFrag = owner.Fragments.Slide_Land.id
			owner.Rollback.tweenFrames = 0
			owner.actor:Trans( owner.Rollback.iSlideFrag, owner.Rollback.tweenFrames, 0)
			owner.effects:StartEffect(3767496809)
			owner.effects:StartEffect(1910235783)
			owner.Rollback.bIsFalling = false
			if(owner.Rollback.fSlideJumpTimer > 0) then
				owner.Rollback.slideVel = vecNew(vecScale(owner.Rollback.slideVel, 0.5))
				--Log("Reduce slide velocity on consecutive hops")
			end
		end
		
		if(owner.Rollback.fsetCharVelocZ < 0) then
			owner.Rollback.fsetCharVelocZ = 0
		end
		
		if(owner.fStickSpeed > owner.fStickDeadzone) then
			owner:SetIntendedFacingAndInputDiff()
		end

		-- Calculate slope direction
		local slopeAngle = owner.actor:GetGroundSlopeAngle();
		local slopeAccelerationDir = {x=0, y=0, z=0}
		if (slopeAngle >= minSlopeAngle) then-- and owner.Rollback.fWallHitTimer <= 0.1) then
			-- We can assume this is non-nil because GetGroundSlopeAngle() only returns > 0 if we're on the ground, meaning our per-frame raycast in self:DistanceToGround() will have hit something
			slopeAccelerationDir = owner.groundFinder:GetGroundNormal()
		else
			slopeAccelerationDir = owner:GetDirectionVector()
		end
		slopeAccelerationDir.z = 0

		-- Accelerate in direction of slope
		if (slopeAngle >= minSlopeAngle) then
			local slopeAcc = slopeAcceleration * elapsed
			owner.Rollback.slideVel.x = owner.Rollback.slideVel.x + slopeAccelerationDir.x*slopeAcc
			owner.Rollback.slideVel.y = owner.Rollback.slideVel.y + slopeAccelerationDir.y*slopeAcc

			-- Cap max speed
			local slideSpeed = vecLen(owner.Rollback.slideVel)
			if (slideSpeed > maxSpeed) then
				NormalizeVector(owner.Rollback.slideVel)
				owner.Rollback.slideVel.x = owner.Rollback.slideVel.x * maxSpeed
				owner.Rollback.slideVel.y = owner.Rollback.slideVel.y * maxSpeed
			end
		end

		-- Steer slide towards stick input
		if (owner.fStickSpeed > owner.fStickDeadzone) then
			local stickWorldDir = NormalizeVector(owner.actor:GetStickWorldDirection(0))
			local upDir = {x=0, y=0, z=1}
			local slopeDir = {x=owner.Rollback.slideVel.x, y=owner.Rollback.slideVel.y, z=0}
			NormalizeVector(slopeDir)
			local slopePerpDir = NormalizeVector(vecCross(slopeDir, upDir))
			local facingSlopeDot = vecDot(stickWorldDir, slopeDir)
			
			if (facingSlopeDot <= math.cos(g_Deg2Rad*5.0)) then
				local facingSlopePerpDot = vecDot(stickWorldDir, slopePerpDir)
				local steerDir = 0
				if (facingSlopePerpDot >= 0) then
					steerDir = -1
				else
					steerDir = 1
				end

				local steerSpeed = steerSpeed * elapsed * steerDir
				local originalSlideVel = {x=owner.Rollback.slideVel.x, y=owner.Rollback.slideVel.y, z=0}
				owner.Rollback.slideVel.x = originalSlideVel.x*math.cos(steerSpeed) - originalSlideVel.y*math.sin(steerSpeed)
				owner.Rollback.slideVel.y = originalSlideVel.x*math.sin(steerSpeed) + originalSlideVel.y*math.cos(steerSpeed)
			end
		end
		
		-- Friction
		if (owner.Rollback.fWallHitTimer >= 0.1) then
			-- When running into a wall, quickly reduce our speed
			local slideVsWallFriction = 20.0
			owner.Rollback.slideVel.x = VariableInterpolate(owner.Rollback.slideVel.x, 0, elapsed, slideVsWallFriction);
			owner.Rollback.slideVel.y = VariableInterpolate(owner.Rollback.slideVel.y, 0, elapsed, slideVsWallFriction);
		else
			-- It felt better when there was very little friction on steep slopes and a quick ramp up to a lot of friction on flat ground, that's what the math.pow() is used for
			local slopeAngleFactor = 1.0 - clamp(slopeAngle / 90.0, 0.0, 1.0)
			local slopeFriction = math.pow(slopeAngleFactor, 15.0)
			
			if(owner.class == "SeaDog" and owner.Rollback.fTreadmillCurBoost > 1.0 and owner.Rollback.fCurLootCount > 0) then
				slopeFriction = slopeFriction * 0.85
			end
			
			if(owner.bIsCarryingLoot) then	
				slopeFriction = slopeFriction*1.5
			end	
			owner.Rollback.slideVel.x = VariableInterpolate(owner.Rollback.slideVel.x, 0, elapsed, slopeFriction);
			owner.Rollback.slideVel.y = VariableInterpolate(owner.Rollback.slideVel.y, 0, elapsed, slopeFriction);
		end
				
		local finalSlideVel = owner.Rollback.slideVel --ScaleVector(owner.Rollback.slideVel, owner.Rollback.fTreadmillCurBoost)	--multiply by treadmill boost modifier
		
		finalSlideVel.z = owner.Rollback.fsetCharVelocZ

		-- Apply velocity
		owner.actor:ClearVelocity()
		owner.actor:SetAdditionalWorldVelocity(finalSlideVel)

		-- Rotate to face slide direction
		local intendedFacingDir = {x=finalSlideVel.x, y=finalSlideVel.y, z=0.0};
		NormalizeVector(intendedFacingDir)
		owner.actor:SetIntendedFacingDir(intendedFacingDir);
		
		owner:RotateTowardsIntendedFacing(rotationSpeed, false, false)
	else
		self:SetStandardGroundAngles(owner)

		if(owner.Rollback.bIsFalling == false and owner.fDistanceToGround > 0.5) then
			owner.Rollback.bIsFalling = true
			owner:InheritVelocity(true)
			if(owner:Pressed_Slide(.1)) then
				if(owner.Rollback.fsetCharVelocZ > -10) then
					owner.Rollback.fsetCharVelocZ = -10.0
				end
			end
		end

		owner:Jump_Physics(true, owner.fSlideGravity, false)
	end

	owner:CheckForLootInput(0.1)
	owner:UpdateSlideDustVFX()
end

function BaseCharacter_StateSlide:DuringCommitted( owner, elapsed )
	owner:UpdateSlideDustVFX()
end

