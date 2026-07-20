Deobfuscated Ardumower Sunray Web App Source
=============================================

Origin: http://grauonline.de/alexwww/ardumower/ardumower-sunray-app
Fetched: 2026-07-20

Files
-----
- `wpplanner.deobf.js`   – waypoint/planner logic (calcPatternRings, calcPatternLines, calc, connectPolysUsingPathFinding)
- `pathfinder.deobf.js`  – BasicPathFinder (findPath, goPathOnPolygon, findPathAstar)
- `map.deobf.js`         – UI map handling, calls into wpplanner/pathfinder

Note
----
The files are still partly obfuscated: variable names remain `_0x...`.
String-literal table accesses have been replaced with their actual strings,
so `console.log(_0xabc('0x0'))` becomes `console.log("calcDifferenceRingsByExclusions")`.
This makes the algorithm flow readable while keeping the original semantics.

For a fully human-readable version run the deobfuscator script on the current
`*.js` files from the Sunray app URL.
