namespace UnrealBuildTool.Rules
{
	public class RegexRename : ModuleRules
	{
		public RegexRename(ReadOnlyTargetRules Target) : base(Target)
		{
			DefaultBuildSettings = BuildSettingsVersion.V2;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"UnrealEd",
				}
			);
		}
	}
}
