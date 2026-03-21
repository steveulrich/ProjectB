BaseCharacter_StateJump = { name = "jump" }

function BaseCharacter_StateJump:IsAllowed( owner )
	-- if((owner:Pressed(513004264, 0) and not owner.actor:IsFragmentCategory(fcat_jump, owner.fullbodyscope_id)) and owner.actor:IsonGround() == false) then  -- temp until we can get attack priority over state enter
		-- return false
	-- end
		
	

	if (owner.bMovementAllowed) then
		if (owner:CanPlayerMove() and owner.fAllowJumpTimer <= 0 and owner.Rollback.bRampBoostActive == false and (owner:Pressed_Jump(.1) and owner.Rollback.fCurJumpCount < owner.fMaxJumpCount and (owner.Rollback.fOffGroundTimer < 0.2
		or (owner.Rollback.bAllowAirJump == true and owner.actor:IsOnGround() == false)) or (owner.actor:IsFragmentCategory(fcat_jump, owner.fullbodyscope_id)))) then

			return true		
			-- if( owner:AttackInputPressed(1) and owner.actor:IsOnGround() == true ) then 
				-- return false
			-- else
				-- return true
			-- end
		end
	end
	
	return false;
end

function BaseCharacter_StateJump:SetAnimFromFullyCommitted( owner )
	if(owner.actor:IsFragmentCategory(fcat_reaction, owner.fullbodyscope_id) or owner.actor:IsFragmentCategory(fcat_attack, owner.fullbodyscope_id)) then
		owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise.id
	else		
		if(owner.fcurCharVelocZ > 0) then
			owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise.id
			owner.Rollback.tweenFrames = 4
		else
			owner.Rollback.iJumpFrag = owner.Fragments.Jump_Falling.id
			owner.Rollback.tweenFrames = 8
		end
	end
	owner.actor:Trans( owner.Rollback.iJumpFrag, owner.Rollback.tweenFrames, 0)	

	if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
		owner:CheckForAttackInput(.2)
	end
	owner:BuildableInput(0.1)
	owner:Building()
	owner:CheckForLootInput(0.1)		
	-- Fix to not being able to jump if you go into a fullycommitted sequence while in jump state then return from committed directly into jump state, don't like this method, but it works for now -- DOH
	if(owner.currentState == BaseCharacter_StateJump) then 
		owner.currentState:Exit( owner )
		owner.currentState:Enter( owner )
	end
	owner:OnReturnFromCommitted()
end

function BaseCharacter_StateJump:SetAnimFromTurnOrLand( owner )
		if(owner.actor:IsFragmentCategory(fcat_reaction, owner.fullbodyscope_id) or owner.actor:IsFragmentCategory(fcat_attack, owner.fullbodyscope_id)) then
			owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise.id
		else		
			if(owner.fcurCharVelocZ > 0) then
				owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise.id
				owner.Rollback.tweenFrames = 4
			else
				owner.Rollback.iJumpFrag = owner.Fragments.Jump_Falling.id
				owner.Rollback.tweenFrames = 8
			end
		end
		if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
			owner:CheckForAttackInput(.2)
		end		
		owner.actor:Trans( owner.Rollback.iJumpFrag, owner.Rollback.tweenFrames, 0)	
--		owner:CheckForAttackInput(.1)
--		owner:BuildableInput(0)
--		owner:CheckForLootInput(0)			
end

function BaseCharacter_StateJump:Enter( owner )
	--owner.fJumpGravity = 25
	owner.Rollback.bleavingground = true
	
--	owner.lootProximityCapsuleRadius = owner.lootProximityCapsuleRadius + .5
--	owner.lootProximityCapsuleHeight = owner.lootProximityCapsuleHeight + .5
	
--	Log( string.format( "StateJumpEnter(%f)", owner.elapsedTime  ) )
	
	owner.Rollback.timeInJump = 0
	owner.Rollback.fFallTimer = 0
	owner.Rollback.isJumping = true
	if (owner.inAirTagId ~= -1) then
		owner.actor:SetTag(owner.inAirTagId);
	end
	--owner.Rollback.fOffGroundTimer = 0
	if(owner.previousState.name ~= "jump_precision") then
		if(owner.Rollback.bAllowAirJump == true and owner.Rollback.fCurJumpCount > 0) then
			owner:JumpStart(owner.fDoubleJumpHeight, false)
		else
			owner:JumpStart()
		end
	else
		owner:InheritVelocity(true)
	end
	owner.Rollback.bOnlyOnce = true
	owner:CheckForJumpStart()
	
	if(owner.actor:IsOnGround() == true or owner.fDistanceToGround < 0.1 or owner.Rollback.fOffGroundTimer < 0.2) then
		owner.Rollback.fCurJumpCount = 1
	elseif(owner.Rollback.bAllowAirJump == true) then
		owner.Rollback.fCurJumpCount = 2
	end

	owner.Rollback.fStuckInFallTime = 0	
	--- Test for long jump --- 
	--if(owner.actor:IsFragmentCategory(fcat_jumpland, owner.fullbodyscope_id)) then
	--	owner.ijumpcounter = owner.ijumpcounter+1
	--else
	--	owner.ijumpcounter = 0
	--end
	owner.effects:StartEffect(2139284529)
	owner.effects:StartEffect(3718367030)
	owner.bJumpDustVFX = true
	owner.Rollback.fJumpCooldownTimer = 0.2

	owner.Rollback.vJumpStartPos = owner:GetPos()
	MetricsReport.PlayerJumpStart(owner.id)
end

function BaseCharacter_StateJump:Exit( owner )
	owner.lootProximityCapsuleRadius = owner.defaultlootProximityCapsuleRadius
	owner.lootProximityCapsuleHeight = owner.defaultlootProximityCapsuleHeight

	owner.Rollback.bleavingground = false
	owner.Rollback.bAllowAirJump = false
	owner.Rollback.isJumping = false
	owner.Rollback.minJumpActive = false
	owner.Rollback.timeInJump = 0	
	owner.Rollback.fFallTimer = 0
	owner.Rollback.bOnlyOnce = false
	owner.Rollback.bIsBouncing = false
	--owner:StopLootAdditive()
	owner.Rollback.bIsFalling = false
	owner:UpdateFallAudio()
	if (owner.inAirTagId ~= -1) then
		owner.actor:ClearTag(owner.inAirTagId);
	end

	owner.Rollback.fStuckInFallTime = 0
	
	if(owner.actor:IsOnGround() == true or owner.fDistanceToGround < 0.1) then
		owner.Rollback.fCurJumpCount = 0
	end
	
	owner.effects:StopEffect(3718367030)
	owner.bJumpDustVFX = false

	MetricsReport.PlayerJumpEnd(owner.id, owner.Rollback.vJumpStartPos)
	owner.Rollback.vJumpStartPos = nil
end

function BaseCharacter_StateJump:Tick( owner, elapsed )
		--Log("iJumpFrag: "..tostring(owner.Rollback.iJumpFrag))
		
		if(owner.bPerformInputCheck) then
			if(owner.Rollback.timeInJump > 0.133) then -- delay attacks out of jump by 4 frames
				owner:CheckForAttackInput(.2)
			end
		end
		owner:UpdateWallCollisionTimer(true, 0.1, 0.3)
		owner:Jump_Physics(false, 0, true)
		if(owner.bPerformInputCheck) then
			owner:BuildableInput(0.1)
		end
		owner:UpdateRelicGrabStatus()
		owner:Building()
		
		-- if(owner.bIsCarryingLoot) then
			-- owner:ThrowLootInput(0)
		-- else
			-- owner:NavSpecialInput(0)
			-- owner:AttackInput_InAir(0) 
			-- --owner:AttackSpecialInput(0)
		-- end	

		if(owner.Rollback.bAllowAirJump == true) then
			if(owner.Rollback.fCurJumpCount < owner.fMaxJumpCount and owner.Rollback.timeInJump > 0.1) then
				if(owner:Pressed_Jump(0) ) then
					owner:JumpStart(owner.fDoubleJumpHeight)
					owner.Rollback.fCurJumpCount = owner.Rollback.fCurJumpCount + 1
					owner.Rollback.iJumpFrag = owner.Fragments.Jump.id
					owner.actor:Trans( owner.Rollback.iJumpFrag, owner.Rollback.tweenFrames, 0)
				end
			end
		end
		
		if(not owner.actor:IsFragmentPlaying( owner.Rollback.iJumpFrag, owner.fullbodyscope_id)) then
			if(not owner.actor:IsFragmentCategory(fcat_jumptransition, owner.fullbodyscope_id)) then
				owner.actor:Trans( owner.Rollback.iJumpFrag, owner.Rollback.tweenFrames, 0)
				owner.Rollback.tweenFrames = 0
			end
		end	
		
		-- local fallAnimStartSpeed = 10
		-- if(owner.Rollback.fCurJumpCount > 1) then
			-- fallAnimStartSpeed = 30
		-- end
		
		if(owner.fcurCharVelocZ < owner.fFallAnimTriggerSpeed and owner.Rollback.timeInJump > .1) then
			if(not owner.actor:IsOnGround()) then
				owner.Rollback.iJumpFrag = owner.Fragments.Jump_Falling.id
				owner.Rollback.tweenFrames = 0
			else  -- land
				if(owner.fStickSpeed > owner.fStickDeadzone) then
					if((owner.class == "Buccaneer" and owner.Rollback.timeInJump < 0.5) or owner.class ~= "Buccaneer") then
						if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
							owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
						else
							owner.Rollback.iJumpFrag = owner.Fragments.Jump_Land_ToRun.id
						end
					else
						if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
							owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
						else
							owner.Rollback.iJumpFrag = owner.Fragments.Jump_Land.id
						end
					end
					owner.Rollback.tweenFrames = 0
				else
					if(owner.class == "Gabriel" and owner.fcurCharVelocZ > -7) then
						owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise_Land_ToRun.id
					else
						owner.Rollback.iJumpFrag = owner.Fragments.Jump_Land.id
					end
					owner.Rollback.tweenFrames = 0
				end
				
				if(owner.actualCharVeloc.z > 0 or owner.Rollback.fsetWorldVelocZ > 0) then
					owner:ResetZVelocity()
				end
				owner.Rollback.fSlideCooldownTimer = 0
			end			
		else
			owner.Rollback.iJumpFrag = owner.Fragments.Jump_Rise.id
			owner.Rollback.tweenFrames = 0
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
	owner.fDontAllowTransitionTimer = owner.fDontAllowTransitionTimer - elapsed

	owner:CheckForLootInput(0.1)
	owner:UpdateFallAudio()
	owner:UpdateJumpDustVFX()
	--owner:CheckPrecisionModeInput()
end

function BaseCharacter_StateJump:DuringCommitted( owner, elapsed )
	if(owner.Rollback.bIsFalling == true) then
		owner.Rollback.bIsFalling = false
	end
	
	owner:UpdateJumpDustVFX()
	owner:UpdateFallAudio()
end

