// Copyright Epic Games, Inc.All Rights Reserved.

using EpicGame;
using Gauntlet;

namespace LyraTest
{
	public sealed class StartElimination : EpicGameTestNode<EpicGameTestConfig>
	{
		public StartElimination(UnrealTestContext InContext) : base (InContext)
		{
		}

		public override EpicGameTestConfig GetConfiguration()
		{
			EpicGameTestConfig Config = base.GetConfiguration();
			Config.NoMCP = true;

			UnrealTestRole Client = Config.RequireRole(UnrealTargetRole.Client);
			Client.Controllers.Add("LyraTestControllerStartEliminationTest");
			
			Client.CommandLineParams.Add("ExecCmds", "Automation RunTest Lyra.MenuStartEliminationSpec");

			return Config;
		}
	}
}
