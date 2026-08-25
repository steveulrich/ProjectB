// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * Alias so UBT/VS can build ProjectBEditor (the target inferred from ProjectB.uproject).
 * Implementation lives in LyraEditor.Target.cs — keep that as the real editor target.
 */
public class ProjectBEditorTarget : LyraEditorTarget
{
	public ProjectBEditorTarget(TargetInfo Target) : base(Target)
	{
	}
}
