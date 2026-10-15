BaseCharacter_StateFall = { name = "fall" }

function BaseCharacter_StateFall:IsAllowed( owner )
-- we shouldn't be checking owner.Rollback.isJumping, that only gets reset on_exit of jump state which 
-- causes attacks to no enter fall state since they are fully committed and it's still true

	-- if(owner:Pressed(513004264, 0) and not owner.actor:IsFragmentCategory(fcat_jump, owner.fullbodyscope_id)) then  -- temp until we can get attack priority over state enter
		-- return false
	-- end
	
	
	if (owner.Rollback.bRampBoostActive or owner.Rollback.bIsBouncing or (owner.actor:IsOnGround() == false and (owner.fDistanceToGround > 0.4 or owner.Rollback.fAirAttackTimer > 0)) or (owner.actor:IsFragmentCategory(fcat_falling, owner.fullbodyscope_id))) then
		return true
	end

	return false;
end

function BaseCharacter_StateFall:SetAnimFromFullyCommitted( owner )
	if(owner.fcurCharVelocZ > 0) then
		owner.Rollback.iFallFrag = owner.Fragments.Falling_Up.id	
		owner.Rollback.tweenFrames = 4
	else
		owner.Rollback.iFallFrag = owner.Fragments.Falling.id	
		owner.Rollback.tweenFrames = 4
	end	
	owner.actor:Trans( owner.Rollback.iFallFrag, owner.Rollback.tweenFrames, 0)	
	if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
		owner:CheckForAttackInput(.2)
	end	
	owner:CheckForLootInput(0.1)	
	-- Fix to not entering/exiting state when going into a fullycommitted sequence then returning from committed back into the same state -- DOH
	if(owner.currentState == BaseCharacter_StateFall) then 
		owner.currentState:Exit( owner )
		owner.Rollback.bSkipTransition = true
		owner.Rollback.bAllowFallTransition = false
		owner.currentState:Enter( owner )
	end
	owner:OnReturnFromCommitted()
end

function BaseCharacter_StateFall:SetAnimFromTurnOrLand( owner )
	if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
		owner:CheckForAttackInput(.2)
	end
end

function BaseCharacter_StateFall:Enter( owner )
	owner:FallStart()
	owner.Rollback.bleavingground = true	
	--owner.Rollback.bIsFalling = true
	owner.Rollback.timeInJump = 0
	owner.Rollback.fFallTimer = 0
	owner:CheckForFallStart()
	if (owner.inAirTagId ~= -1) then
		owner.actor:SetTag(owner.inAirTagId);
	end
end

function BaseCharacter_StateFall:Exit( owner )
	--owner.Rollback.bIsFalling = false
	owner.Rollback.timeInJump = 0	
	owner.Rollback.fFallTimer = 0
	owner.Rollback.bIsBouncing = false
	owner.Rollback.bAllowFallTransition = true
	--owner:StopLootAdditive()
	owner.Rollback.bIsFalling = false
	owner.bAllowJumpBufferIncrement = true
	owner:UpdateFallAudio()
	if (owner.inAirTagId ~= -1) then
		owner.actor:ClearTag(owner.inAirTagId);
	end
	if(owner.actor:IsOnGround() == true or owner.fDistanceToGround < 0.1) then
		owner.Rollback.fCurJumpCount = 0
	end
end

function BaseCharacter_StateFall:Tick( owner, elapsed )
	
	-- if(owner.bIsCarryingLoot) then
		-- owner:ThrowLootInput(0)
	-- else
		-- owner:NavSpecialInput(0)
		-- owner:AttackInput_InAir(0)
		-- --owner:AttackSpecialInput(0)
	-- end

	if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
		owner:CheckForAttackInput(.2)
	end
	owner:UpdateRelicGrabStatus()
	
	owner:UpdateWallCollisionTimer(true, 0.8, 0.3)
	--owner:CheckWallJumpInput()
			
	if(not owner.actor:IsFragmentPlaying( owner.Rollback.iFallFrag, owner.fullbodyscope_id)) then
		owner.actor:Trans( owner.Rollback.iFallFrag, owner.Rollback.tweenFrames, 0)	
		owner.Rollback.tweenFrames = 0
	end	
	
	if(owner.actualCharVeloc.z < 10) then
		if(not owner.actor:IsOnGround()) then
			if(owner.Rollback.bAllowFallTransition) then
				owner:CheckForFallStart()
			else
				owner.Rollback.iFallFrag = owner.Fragments.Falling.id
				owner.Rollback.tweenFrames = 8
			end
		elseif(owner.Rollback.bRampBoostActive == false and owner.Rollback.bIsBouncing == false) then  -- land
			if(owner.fStickSpeed > owner.fStickDeadzone) then
				if((owner.class == "Buccaneer" and owner.Rollback.timeInJump < 0.5) or owner.class ~= "Buccaneer") then
					if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
						owner.Rollback.iFallFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
					else
						owner.Rollback.iFallFrag = owner.Fragments.Jump_Land_ToRun.id
					end
					owner.Rollback.tweenFrames = 0
				else
					if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
						owner.Rollback.iFallFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
					else
						owner.Rollback.iFallFrag = owner.Fragments.Jump_Land.id
					end
					owner.Rollback.tweenFrames = 0
				end
			else
				if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
					owner.Rollback.iFallFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
				else
					owner.Rollback.iFallFrag = owner.Fragments.Jump_Land.id
				end
				owner.Rollback.tweenFrames = 0
			end
			owner.Rollback.fSlideCooldownTimer = 0
			owner.Rollback.bAllowFallTransition = true
		end			
	else
		owner.Rollback.iFallFrag = owner.Fragments.Falling_Up.id	
		owner.Rollback.tweenFrames = 4
		owner.Rollback.bAllowFallTransition = true
	end
		
	if(owner.actor:IsOnGround() == false or owner.Rollback.bRampBoostActive or owner.Rollback.bIsBouncing) then
		owner:Jump_Physics(false, 0, true)
	else
		if(owner.Rollback.fOnGroundTimer < 3.0) then	--fix for Z velocity getting reset before it can actually be applied when transitioning from nav or idle or slide
			owner.Rollback.fsetCharVelocZ = 0
			--Log("fOnGroundTimer: %f", owner.Rollback.fOnGroundTimer)
		end

		local velocity = {x=owner.Rollback.fsetWorldVelocX, y=owner.Rollback.fsetWorldVelocY, z=owner.Rollback.fsetCharVelocZ}
		owner.actor:ClearVelocity()
		owner.actor:SetAdditionalWorldVelocity(velocity)
	end
		
	owner.Rollback.timeInJump = owner.Rollback.timeInJump + elapsed
	if(owner.fcurCharVelocZ < 0) then
		owner.Rollback.fFallTimer = owner.Rollback.fFallTimer + elapsed
		if(owner.Rollback.bIsFalling == false) then
			owner.Rollback.bIsFalling = true
		end
	else
		owner.Rollback.fFallTimer = 0
		if(owner.Rollback.bIsFalling == true) then
			owner.Rollback.bIsFalling = false
		end
	end
	
	owner:CheckForLootInput(0.1)
	owner:UpdateFallAudio()

end

function BaseCharacter_StateFall:DuringCommitted( owner, elapsed )
	if(owner.Rollback.bIsFalling == true) then
		owner.Rollback.bIsFalling = false
	end
	
	owner:UpdateFallAudio()
end


