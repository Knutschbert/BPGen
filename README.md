# BPGen
A modified version of the [BPGen UE4 Plugin](https://github.com/trumank/drg-mods/blob/main/Plugins/BPGen/Source/BPGen/Private/BPGen.cpp) by @trumank 

## Example output
```json

{
	"classes": {
		"/Script/Engine": {
			"ReceiveActorOnInputTouchLeave": {
				"pure": false,
				"tooltip": "Event when this actor has a finger moved off of it with the clickable interface.",
				"FMeta_DisplayName": "TouchLeave",
				"pins": [
					{
						"name": "FingerIndex",
						"type": "TEnumAsByte<ETouchIndex::Type>",
						"type_parsed": {
							"OuterType": "TEnumAsByte",
							"InnerType": "ETouchIndex::Type",
							"IsPointer": false
						},
						"direction": "input",
						"isRef": false
					}
				]
			}
		},
		"/Script/GameplayTags": {
			"GetAllActorsOfClassMatchingTagQuery": {
				"pure": false,
				"tooltip": "Get an array of all actors of a specific class (or subclass of that class) which match the specified gameplay tag query.\n\n",
				"FMeta_Tooltip": "Get an array of all actors of a specific class (or subclass of that class) which match the specified gameplay tag query.\n\n@param ActorClass                    Class of actors to fetch\n@param GameplayTagQuery              Query to match against",
				"pins": [
					{
						"name": "WorldContextObject",
						"type": "UObject*",
						"type_parsed": {
							"OuterType": "UObject",
							"InnerType": "",
							"IsPointer": true
						},
						"direction": "input",
						"isRef": false
					},
					{
						"name": "ActorClass",
						"type": "TSubclassOf<AActor>",
						"type_parsed": {
							"OuterType": "TSubclassOf",
							"InnerType": "AActor",
							"IsPointer": false
						},
						"direction": "input",
						"isRef": false,
						"tooltip": "Class of actors to fetch"
					},
					{
						"name": "GameplayTagQuery",
						"type": "FGameplayTagQuery",
						"type_parsed": {
							"OuterType": "FGameplayTagQuery",
							"InnerType": "",
							"IsPointer": false
						},
						"direction": "input",
						"isRef": true,
						"tooltip": "Query to match against"
					},
					{
						"name": "OutActors",
						"type": "TArray",
						"type_parsed": {
							"OuterType": "TArray",
							"InnerType": "",
							"IsPointer": false
						},
						"direction": "output",
						"isRef": false
					}
				]
			}
		}
	}
}

```