

// Some copyright should be here...

using UnrealBuildTool;

public class ManualVRPlugin : ModuleRules
{
    public ManualVRPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
        );

        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
        );

        // --- CLEANED UP DEPENDENCIES ---
        // We put the core modules and the HTTP/JSON modules here so they are available everywhere in your plugin.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "HeadMountedDisplay",
                "HTTP",
                "Json",
                "JsonUtilities"
            }
        );

        // We leave only the UI/Slate stuff here, and remove the duplicates.
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
        );
    }
}