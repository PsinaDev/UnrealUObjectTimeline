# UnrealUObjectTimeline

An Unreal Engine 5.6+ plugin that provides Timeline functionality for any UObject-derived class, breaking free from the standard Actor-only `UTimelineComponent` limitation.

## Installation
1. Clone this repository into your project's `Plugins` folder
2. Regenerate project files
3. Build the project
___
OR
___
1. Download packaged version of the plugin in releases.
2. Copy to your project's `Plugins` folder
3. Enjoy!
## Module Structure

| Module | Type | Description |
|--------|------|-------------|
| `ObjectTimelineRuntime` | Runtime | Core `UTimelineObject` class and binding system |
| `ObjectTimelineUncooked` | Editor | Blueprint node, graph widget, and timeline editor |

## Usage

### In Blueprint

1. Right-click in the Blueprint graph
2. Search for "Object Timeline" under the Object Timeline category
3. Add the node and connect execution pins
4. Double-click the node to open the Timeline Editor
5. Add tracks and keyframes as usual

### In C++
```cpp
#include "TimelineObject.h"

// Get or create a timeline for any UObject
UTimelineObject* Timeline = UTimelineObject::GetOrCreateTimelineObject(
    this,                    // Owner (any UObject)
    TEXT("MyTimeline"),      // Timeline name
    TEXT("OnTimelineUpdate"),// Update callback function name (optional)
    TEXT("OnTimelineEnd")    // Finished callback function name (optional)
);

// Control playback
Timeline->Play();
Timeline->PlayFromStart();
Timeline->Reverse();
Timeline->Stop();

// Query state
float Position = Timeline->GetPlaybackPosition();
bool bPlaying = Timeline->IsPlaying();

// Get interpolated values
float FloatValue = Timeline->GetFloatValue(MyCurveFloat);
FVector VectorValue = Timeline->GetVectorValue(MyCurveVector);
```

## Architecture
```
UTimelineObject (UObject + FTickableGameObject)
├── FTimeline (internal timeline logic)
├── Track Curves (Float, Vector, Color, Event)
├── Event Track Delegates
└── Bound Function Tracking

UK2Node_TimelineObject (Blueprint Node)
├── Input Pins (Play, Stop, Reverse, SetNewTime)
├── Output Pins (Update, Finished, Direction)
├── Track Output Pins (dynamic per track)
└── ExpandNode() → GetOrCreateTimelineObject + Event Nodes

UTimelineObjectBinding (UDynamicBlueprintBinding)
└── Automatic delegate binding for compiled Blueprints
```

## Key Differences from UTimelineComponent

| Feature | UTimelineComponent | UTimelineObject |
|---------|-------------------|-----------------|
| Owner Type | Actor only | Any UObject |
| Ticking | Component tick | FTickableGameObject |
| Blueprint Node | UK2Node_Timeline | UK2Node_TimelineObject |
| Delegate Binding | Automatic (Actor) | UTimelineObjectBinding |
| World Access | Actor->GetWorld() | Outer->GetWorld() |

## Requirements

- Unreal Engine 5.6+
- C++17 or later

## License
MIT License.

## Contributing

Issues and pull requests are welcome.
