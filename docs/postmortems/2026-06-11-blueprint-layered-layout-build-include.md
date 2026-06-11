# Postmortem: Blueprint Layered Layout Build Include

## Symptom
`python tools\build_plugin.py --engine-root "C:\Program Files\Epic Games\UE_5.8"` failed while compiling `LayeredGraphLayout.cpp`.

## Impact
The new layered layout core could not package as part of ECABridge for UE 5.8.

## Root Cause
`Source/ECABridge/Private/Commands/BlueprintLayout/LayeredGraphLayout.cpp` included `BlueprintLayout/LayeredGraphLayout.h`. For a source file already inside `BlueprintLayout`, UBT did not add the parent `Commands` folder to that translation unit's quoted-include search path.

## Fix
Changed the include to `#include "LayeredGraphLayout.h"` in `LayeredGraphLayout.cpp`.

## Regression Test
```powershell
python tools\build_plugin.py --engine-root "C:\Program Files\Epic Games\UE_5.8"
```

## Debug Entry
- Log keyword: `Cannot open include file: 'BlueprintLayout/LayeredGraphLayout.h'`
- Artifact path: `artifacts/build-plugin/ue5.8-20260611-055222/`
- Relevant file: `Source/ECABridge/Private/Commands/BlueprintLayout/LayeredGraphLayout.cpp`
- Related test: `python tools\build_plugin.py --engine-root "C:\Program Files\Epic Games\UE_5.8"`

## Notes
For private source files in nested folders, prefer same-folder quoted includes or add an explicit module include path.