

using UnrealBuildTool;

public class AzurealXR : ModuleRules
{
    public AzurealXR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// Add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// Add other private include paths required here ...
			}
            );


        // FIXED: Capitalized "PublicDependencyModuleNames"
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "HeadMountedDisplay",
                "EnhancedInput",
                "XRBase",
                "UMG",
                "Slate",
                "SlateCore",
                "NavigationSystem",
                "CableComponent",
                "HTTP", // Required for API communication
			    "Json", // Required for parsing server responses
			    "JsonUtilities"
            }
            );


        // FIXED: Capitalized "PrivateDependencyModuleNames"
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
				// ...
			}
            );
    }
}