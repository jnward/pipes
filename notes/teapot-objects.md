# 3D Pipes (NT4 SDK) — Teapot easter egg, OBJECTS.CXX/NODE.CXX geometry, scene reset

Source root: `/home/user/pipes/original/MSTOOLS/SAMPLES/OPENGL/SCRSAVE` (all paths below relative to that unless absolute). All line numbers verified against the tree at commit b79e6a5 ("Add pristine 3D Pipes source from Windows NT 4.0 SDK (Aug 1996)").

---

## (a) Teapot easter egg — complete mechanism

### grep -ri teapot hits (PIPES + COMMON)

| File:line | Content |
|---|---|
| `COMMON/MATERIAL.C:40` | `// 'tea' materials, from aux teapots program` |
| `COMMON/MATNAME.H:3` | `// 24 tea materials (from teapots app)` |
| `PIPES/PIPE.H:75` | `void DrawTeapot();` (protected member of base class `PIPE`) |
| `PIPES/PIPE.CXX:54,61,65,69` | `DrawTeapot` implementation, `auxSolidTeapot(2.5 * radius)` |
| `PIPES/STATE.H:22` | `#define TEAPOT 66` |
| `PIPES/NSTATE.CXX:187–189` | `// draw a teapot once in a blue moon` → `return( TEAPOT );` |
| `PIPES/NPIPE.CXX:93` | `DrawTeapot();` (stuck at start node) |
| `PIPES/NPIPE.CXX:368–369` | `// Horrors! It's the teapot!` → `DrawTeapot();` (joint replacement) |
| `PIPES/FPIPE.CXX:242` | `DrawTeapot();` (`REGULAR_FLEX_PIPE::Start` stuck) |
| `PIPES/FPIPE.CXX:314` | `DrawTeapot();` (`TURNING_FLEX_PIPE::Start` stuck) |

Note: the teapot is **not** drawn in OBJECTS.CXX at all. OBJECTS.CXX contains no teapot code; the drawing lives in the base class **`PIPE::DrawTeapot()`** in `PIPES/PIPE.CXX:60-73`.

### Data source: aux library teapot (evaluators), NOT embedded patch data

`PIPES/PIPE.CXX:58-73`:

```cpp
extern void ResetEvaluator( BOOL bTexture );

void
PIPE::DrawTeapot( )
{
    glFrontFace( GL_CW );
    glEnable( GL_NORMALIZE );
    auxSolidTeapot(2.5 * radius);
    glDisable( GL_NORMALIZE );
    glFrontFace( GL_CCW );
    if( type != TYPE_NORMAL ) {
        // Re-init flex's evaluator state (teapot uses evaluators as well,
        //  and messes up the state).
        ResetEvaluator( bTexture );
    }
}
```

- The geometry comes from **`auxSolidTeapot()`** in the OpenGL **aux** library (`<GL/glaux.h>` is included from `PIPES/SSPIPES.H:15`; linked from GLAUX.LIB). There is no patch data embedded in the screensaver source — this is the only call site in the whole SCRSAVE tree (grep for `auxSolidTeapot` matches only `PIPES/PIPE.CXX:65`). **For the WASM port, `auxSolidTeapot` is the one piece of external geometry that must be supplied** (glaux's teapot renders the classic Newell teapot patch set via `glMap2f`/`glEvalMesh2` evaluators, scale = `2.5 * radius` = 2.5 with `radius = 1.0f` set in `STATE.CXX:72`).
- `glFrontFace(GL_CW)` is needed because the aux teapot is wound clockwise while pipes use CCW (`STATE.CXX:262 glFrontFace(GL_CCW)`); culling is on (`STATE.CXX:280-281`).
- `GL_NORMALIZE` is enabled just for the teapot (scaling in aux teapot's modelview ops would corrupt lighting otherwise).
- Because the aux teapot itself uses 2D evaluators, after drawing it in **flex** mode (`type != TYPE_NORMAL`) the pipe's own evaluator state must be re-armed via **`ResetEvaluator(bTexture)`** — `PIPES/EVAL.CXX:103-110`:

```cpp
void
ResetEvaluator( BOOL bTexture )
{
    if( bTexture ) {
        glEnable( GL_MAP2_TEXTURE_COORD_2 );
    }
    glEnable( GL_MAP2_VERTEX_3 );
    glEnable( GL_AUTO_NORMAL );
```

(Also note `STATE::GLInit` enables `GL_AUTO_NORMAL` globally with the comment `// needed for GL_MAP2_VERTEX (tea)` — `STATE.CXX:267`.)

### Trigger 1 — probabilistic joint replacement (the famous one): NSTATE.CXX

`PIPES/NSTATE.CXX:176-197`:

```cpp
#define BLUE_MOON 153

int
NORMAL_STATE::ChooseJointType( )
{
    switch( jointStyle ) {
        case ELBOWS:
            return ELBOW_JOINT;
        case BALLS:
            return BALL_JOINT;
        case EITHER:
            // draw a teapot once in a blue moon
            if( ss_iRand(1000) == BLUE_MOON )
                return( TEAPOT );
        default:
            // otherwise an elbow or a ball (1/3 ball)
            if( !ss_iRand(3) )
                return BALL_JOINT;
            else
                return ELBOW_JOINT;
    }
}
```

Conditions, exactly:
- Only reachable when `jointStyle == EITHER`, i.e. the user chose **JOINT_MIXED**, or **JOINT_CYCLE** during frames where the cycling style has come around to `EITHER` (see part (c) below; mapping happens in the `NORMAL_STATE` constructor, `NSTATE.CXX:35-51`). With `JOINT_ELBOW` or `JOINT_BALL` settings a teapot joint can never appear.
- Probability: `ss_iRand(1000) == 153` → **1/1000 per joint drawn** (per turn). `ss_iRand(max)` returns 0..max-1 (`COMMON/UTIL.CXX:115-118`: `return (int)(max * (((float)rand()) / ((float)(RAND_MAX+1))));`).
- Only NORMAL (display-list) pipes draw joints; `ChooseJointType` is only called from `NORMAL_PIPE::DrawJoint` (`NPIPE.CXX:307`). Texture mode does **not** disable it — it changes which joints are drawn for ELBOW/BALL results but the `TEAPOT` return happens before that branch.

Consumption in `PIPES/NPIPE.CXX:301-373` (`NORMAL_PIPE::DrawJoint`):

```cpp
    jointType = pNState->ChooseJointType();
    ...
    switch( jointType ) {
      case BALL_JOINT:
            ...
      case ELBOW_JOINT:
            ...
      default:
            // Horrors! It's the teapot!
            DrawTeapot();
            align_plusz( newDir );
            // move ahead 1.0*r to draw pipe
            glTranslatef( 0.0f, 0.0f, radius );
        }
```

`TEAPOT` (66) matches neither `BALL_JOINT` (1) nor `ELBOW_JOINT` (0) (`NSTATE.H:31-34`), so it lands in `default:`. The teapot is drawn **in place of the joint**, un-rotated (in node-grid orientation, before `align_plusz`), and the pipe then continues out of it in the new direction.

### Trigger 2 — stuck at start node (all pipe types)

If a pipe's very first `ChooseNewDirection()` returns `DIR_NONE` (all 6 neighbors taken/out of bounds), a teapot is drawn at the start node instead of a start cap:
- `NORMAL_PIPE::Start`, `NPIPE.CXX:90-96`:

```cpp
    if( newDir == DIR_NONE ) {
        // pipe is stuck at the start node, draw something
        status = PIPE_STUCK;
        DrawTeapot();
        glPopMatrix();
        return;
    }
```

- `REGULAR_FLEX_PIPE::Start`, `FPIPE.CXX:239-244` (`// draw like one of those tea-pouring thingies...`) and `TURNING_FLEX_PIPE::Start`, `FPIPE.CXX:310-316` — identical pattern. These are the only teapot paths reachable in flex mode, and they are unconditional (no dice roll) once stuck.

### Material handling

There is no special "teapot material". The teapot simply renders with the pipe's current material, chosen at pipe start by `PIPE::ChooseMaterial()` (`PIPES/PIPE.CXX:43-50`):

```cpp
void
PIPE::ChooseMaterial( )
{
    if( bTexture )
        ss_RandomTexMaterial( TRUE );
    else
        ss_RandomTeaMaterial( TRUE );
}
```

The "tea" in `ss_RandomTeaMaterial` refers to the materials' *origin* — they were lifted from the SGI `teapots` aux demo:
- `COMMON/MATNAME.H:5` — `#define NUM_TEA_MATERIALS 24`, enum `EMERALD..YELLOW_RUBBER` (MATNAME.H:7-32), plus 4 white `NUM_TEX_MATERIALS` for texturing (MATNAME.H:36-43).
- `COMMON/MATERIAL.C:41-92` — `static GLfloat teaMaterialData[NUM_TEA_MATERIALS][10]` = ambient(3), diffuse(3), specular(3), shininess; `// 'tea' materials, from aux teapots program` (line 40).
- `COMMON/MATERIAL.C:23-30` — only 16 of the 24 are used:

```c
#define NUM_GOOD_MATERIALS 16  // 'good' ones among the 24 tea materials
int goodMaterials[NUM_GOOD_MATERIALS] = {
        EMERALD, JADE, PEARL, RUBY, TURQUOISE, BRASS, BRONZE,
        COPPER, GOLD, SILVER, CYAN_PLASTIC, WHITE_PLASTIC, YELLOW_PLASTIC,
        CYAN_RUBBER, GREEN_RUBBER, WHITE_RUBBER };
```

- `ss_RandomTeaMaterial(BOOL bSet)` — `MATERIAL.C:289-299`: `index = goodMaterials[ ss_iRand(NUM_GOOD_MATERIALS) ];` then `ss_SetMaterial(&Material[index])` if `bSet`. `ss_SetMaterial` (`MATERIAL.C:194-204`) issues `glMaterialfv` for GL_AMBIENT/GL_DIFFUSE/GL_SPECULAR front+back and `GL_SHININESS = specExp*128.0f`.
- `ss_RandomTexMaterial` (`MATERIAL.C:325-335`) picks one of the 4 whitish materials at `Material[NUM_TEA_MATERIALS + ss_iRand(NUM_TEX_MATERIALS)]`.
- Init: `STATE::STATE` calls `ss_InitTexMaterials()` or `ss_InitTeaMaterials()` (`STATE.CXX:111-114`); those fill the shared `MATERIAL Material[24+4]` array (`MATERIAL.C:111,167-185`), with all alphas forced to 0.5 in `InitMaterials` (`MATERIAL.C:125-145`).

So a joint-teapot appears in whatever material its host pipe is using (any of the 16 "good" tea materials, or a white tex material with the texture applied — the aux teapot generates texture coordinates via `GL_MAP2_TEXTURE_COORD_2`).

---

## (b) OBJECTS.CXX — every geometry builder

Common infrastructure:
- **Everything in OBJECTS.CXX is a hand-built mesh compiled into a display list. No gluQuadric/gluCylinder/gluSphere objects are used anywhere** (grep for `gluQuadric|gluCylinder|gluSphere|gluNewQuadric` over the whole SCRSAVE tree returns nothing; the pipe/sphere builders are hand-inlined *copies* of the GLU quadric tessellation code — see the `// 'glu' routines` comment at `OBJECTS.CXX:468` and identical `sinCache`/`cosCache` structure to GLU's `gluCylinder`/`gluSphere`).
- Base class `OBJECT` (`OBJECTS.H:31-40`, `OBJECTS.CXX:26-50`): constructor does `listNum = glGenLists(1)`, destructor `glDeleteLists(listNum,1)`, and `OBJECT::Draw()` is just `glCallList(listNum)`. Each `Build` wraps its immediate-mode geometry in `glNewList(listNum, GL_COMPILE) ... glEndList()`.
- `OBJECT_BUILD_INFO` (`OBJECTS.H:14-21`): `{ float radius; float divSize; int nSlices; BOOL bTexture; IPOINT2D *texRep; }`.
- `#define CACHE_SIZE 100` (`OBJECTS.CXX:191`) clamps slices/stacks (`if (slices >= CACHE_SIZE) slices = CACHE_SIZE-1;` etc., e.g. lines 250-251, 369-370, 506-507, 584-585).
- Helper functions: `TransformCircle` (`OBJECTS.CXX:106-138`, rotates a circle of points about the x-axis around an anchor using `ss_matrix*` from COMMON/MATH.C), `CalcNormals` (`OBJECTS.CXX:140-151`, normal = point − circle center, normalized), and `MakeQuadStrip` (`OBJECTS.CXX:161-189`, emits `GL_QUAD_STRIP` with per-vertex `glNormal3fv`, optional `glTexCoord2f`).

### 1. PIPE_OBJECT — straight cylinder (`PIPE_OBJECT::Build`, OBJECTS.CXX:485-552)
- Hand-built copy of gluCylinder: precomputes `sinCache/cosCache[slices]` (lines 513-524), then for each of `stacks` z-bands emits a `GL_QUAD_STRIP` ring (lines 528-549), radius constant, `zNormal = 0.0f` (line 509), normals `(sin,cos,0)`.
- Tessellation: `slices = pBuildInfo->nSlices; stacks = (int) SS_ROUND_UP((length/pBuildInfo->divSize) * (float)slices);` (lines 503-504) — stacks scale with length so texture/stack density is uniform.
- Texture: s runs `s_start→s_end` along z per stack, t = `i * texRep->y / slices` around the circumference (lines 535-544).
- Two instances built in `NSTATE.CXX::BuildObjects`: `shortPipe` with length `divSize - 2*radius` (= 5.0) and `longPipe` with length `divSize` (= 7.0); textured versions get `s_trans = s_max * 2.0f * radius / divSize` continuation coords (`NSTATE.CXX:104-111,133-134`).

### 2. ELBOW_OBJECT — 90° torus segment (`ELBOW_OBJECT::Build`, OBJECTS.CXX:225-330)
- Hand-built torus-elbow: starts with a circle of `slices+1` points in the y=r plane centered at (0, r, −r) (lines 270-276), then sweeps it CW around the x-axis through 90° in `stacks` steps using `TransformCircle` (angle `-0.5f * PI * i / stacks`, line 306), computing per-ring normals from the moving ring center and emitting quad strips via `MakeQuadStrip` (lines 304-327). Ends as a circle in the z=0 plane centered at origin, so an elbow "mates" with a cylinder drawn along +z.
- Tessellation: `slices = pBuildInfo->nSlices; stacks = slices / 2;` (`OBJECTS.CXX:247-248`).
- **4 variants** are built (notch = 0..3): `startAng = notch * PI / 2` (line 265) rotates the ring's seam ("notch") in 90° steps so texture seams line up with the adjoining cylinder regardless of turn orientation (big header comment, lines 194-224). `NORMAL_STATE` holds `elbows[4]` (`NSTATE.H:60`), built at `NSTATE.CXX:114-115,135-136`.
- Texture: t around ring (`tex_t[i] = i * texRep->y / slices`, lines 256-259), s from `s_start` to `s_end` along the sweep (line 316).

### 3. BALLJOINT_OBJECT — texture-friendly sphere joint (`BALLJOINT_OBJECT::Build`, OBJECTS.CXX:340-466)
- Same sweep skeleton as the elbow, but each swept ring's radius follows a sphere: `r[i] = sin(angle) * ballRadius` with the first ring at 45° and `ballRadius = ROOT_TWO * radius` (lines 375-381); ring center constant at (0,0,−r) (lines 394-396). Emits quad strips like the elbow.
- Tessellation: `slices = pBuildInfo->nSlices; stacks = slices;` (lines 366-367) — double the elbow's stacks for the tighter curvature.
- 4 notch variants like elbows (line 399 `startAng = notch * PI / 2`). **Built only in texture mode** (`NSTATE.CXX:116`); in non-texture mode `ballJoints[i] = NULL` (`NSTATE.CXX:137`) and a plain `bigBall` sphere is used instead. Used by `NORMAL_PIPE::DrawJoint` BALL_JOINT case when `bTexture` (`NPIPE.CXX:314-330`).

### 4. SPHERE_OBJECT — balls and end caps (`SPHERE_OBJECT::Build`, OBJECTS.CXX:559-712)
- Hand-built copy of gluSphere (lat/long): sin/cos caches for slices and stacks (lines 599-621); in **non-texture** mode poles are `GL_TRIANGLE_FAN`s (lines 630-672, comment at 625-629 explains fans are skipped when texturing because apex tex coords differ per adjacent vertex) and the body is `GL_QUAD_STRIP` bands (lines 677-709); in texture mode quad strips cover all stacks (`start = 0; finish = stacks`, lines 673-676).
- Tessellation: `slices = pBuildInfo->nSlices; stacks = slices;` (lines 582-583).
- s-coordinate sense is inverted relative to the cylinder (lines 587-594, comment `// invert sense of s ...`).
- Instances (`NSTATE.CXX::BuildObjects`):
  - Texture mode: `ballCap = new SPHERE_OBJECT(pBuildInfo, ROOT_TWO*radius, s_start, s_end)` with s range computed to blend with pipe texturing (`NSTATE.CXX:121-129`); `bigBall = NULL` (line 119).
  - Non-texture mode: `bigBall = new SPHERE_OBJECT(pBuildInfo, ROOT_TWO*radius / ((float)cos(PI/nSlices)))` — slightly enlarged `// to prevent any pipe edges from 'sticking' out of the ball` (`NSTATE.CXX:140-144`); `ballCap = new SPHERE_OBJECT(pBuildInfo, ROOT_TWO*radius)` (line 147).
  - `bigBall` doubles as the untextured BALL_JOINT (`NPIPE.CXX:331-335`) and start/end cap (`NPIPE.CXX:185-189, 199-218`); `ballCap` is the textured cap.

### fTesselFact plumbing (dialog → nSlices → builders)
1. Registry: `PIPES/DIALOG.C:89-91` — `tessel = ss_GetRegistryInt(IDS_TESSELATION, 0); SS_CLAMP_TO_RANGE2(tessel, 0, 200); fTesselFact = (float)tessel / 100.0f;` (declared `float fTesselFact = 1.0f;` at DIALOG.C:47; comment at 44-45: "varies from very course (0.0) to very fine (2.0)").
2. `STATE::STATE`, `STATE.CXX:74-77`:

```cpp
    // convert tesselation from fTesselFact(0.0-2.0) to tessLevel(0-MAX_TESS)
    int tessLevel = (int) (fTesselFact * (MAX_TESS+1) / 2.0001f);
    nSlices = (tessLevel+2) * 4;
```

with `#define MAX_TESS 3` (`STATE.H:24`) → tessLevel 0..3 → **nSlices ∈ {8, 12, 16, 20}** (default fTesselFact=1.0 → tessLevel=2 → nSlices=16).
3. `NORMAL_STATE::BuildObjects(pState->radius, pState->view.divSize, pState->nSlices, ...)` (`NSTATE.CXX:55-56`) copies it into `OBJECT_BUILD_INFO->nSlices` (`NSTATE.CXX:95-99`), which every `Build` reads as shown above. (Flex pipes independently consume `pState->nSlices` in EVAL.CXX via `pEval->uDiv = nSlices / pEval->numSections`, `EVAL/FPIPE` path — `FPIPE.CXX:180-181`.)

---

## (c) NODE.CXX — array sizing, direction weighting, joint selection

### Node array sizing from screen size (NUM_DIV)

- `#define NUM_DIV 16 // divisions in window in longest dimension` — `PIPES/SSPIPES.H:52`; `#define NUM_NODE (NUM_DIV - 1) // num nodes in longest dimension` — `PIPES/NODE.H:16`.
- `VIEW::VIEW()` (`VIEW.CXX:24-46`): `numDiv = NUM_DIV; divSize = 7.0f; zTrans = -75.0f; persp.viewAngle = 90.0f;`.
- Sizing happens in `VIEW::CalcNodeArraySize` (`VIEW.CXX:94-114`), driven by window aspect ratio (`aspectRatio = width/height`, set in `SetWinSize`, `VIEW.CXX:134`):

```cpp
    if( winSize.width >= winSize.height ) {
        pNodeDim->x = numDiv - 1;
        pNodeDim->y = (int) (pNodeDim->x / aspectRatio) ;
        if( pNodeDim->y < 1 )  pNodeDim->y = 1;
        pNodeDim->z = pNodeDim->x;
    }
    else {
        pNodeDim->y = numDiv - 1;
        pNodeDim->x = (int) (aspectRatio * pNodeDim->y);
        if( pNodeDim->x < 1 )  pNodeDim->x = 1;
        pNodeDim->z = pNodeDim->y;
    }
```

So the longest screen dimension always gets `NUM_DIV−1 = 15` nodes, the other axis is scaled by aspect ratio, and depth (z) equals the longest dimension (e.g. 4:3 landscape → 15×11×15). Call chain: `STATE::Reshape` sets `RESET_RESIZE_BIT` (`STATE.CXX:343-348`) → `STATE::ResetView` (`STATE.CXX:357-378`) calls `view.CalcNodeArraySize(&numNodes); nodes->Resize(&numNodes);`. `NODE_ARRAY::Resize` (`NODE.CXX:55-88`) allocates `numNodes.x*y*z` `Node`s (1 `GLboolean` each, `NODE.H:23-30`), marks all empty, and precomputes `nodeDirInc[]` strides (x:1, y:numNodes.x, z:numNodes.x*numNodes.y).

### Straight-vs-turn weighting

Per-pipe weight chosen in the `NORMAL_PIPE` constructor, `NPIPE.CXX:47-52`:

```cpp
    // choose weighting of going straight
    if( ! ss_iRand( 20 ) )
        weightStraight = ss_iRand2( MAX_WEIGHT_STRAIGHT/4, MAX_WEIGHT_STRAIGHT );
    else
        weightStraight = 1 + ss_iRand( 4 );
```

with `#define MAX_WEIGHT_STRAIGHT 100` (`NODE.H:19`). I.e. 1-in-20 pipes are "mostly straight" (weight 25..100 inclusive, via `ss_iRand2(min,max)` = min..max, `UTIL.CXX:127-138`); the other 19/20 get weight 1..4. (Base `PIPE` ctor defaults `weightStraight = 1`, `PIPE.CXX:35`; `REGULAR_FLEX_PIPE` uses the same scheme except `weightStraight = ss_iRand(4)` i.e. 0..3, `FPIPE.CXX:114-117`.)

The weight is consumed by `NODE_ARRAY::ChooseRandomDirection` (`NODE.CXX:128-171`), reached via `PIPE::ChooseNewDirection` → `CHOOSE_DIR_RANDOM_WEIGHTED` (`PIPE.CXX:95-112`):

```cpp
    // Get node in straight direction if necessary
    if( weightStraight && nNode[dir] && nNode[dir]->IsEmpty() ) {
        straightNode = nNode[dir];
        // if maximum weight, choose and return
        if( weightStraight == MAX_WEIGHT_STRAIGHT ) {
            straightNode->MarkAsTaken();
            return dir;
        }
    } else
        weightStraight = 0;

    // Get directions of possible turns
    numEmpty = GetEmptyTurnNeighbours( nNode, emptyDirs, dir );

    // Make a random choice
    if( (choice = (weightStraight + numEmpty)) == 0 )
        return DIR_NONE;
    choice = ss_iRand( choice );

    if( choice < weightStraight ) {
        straightNode->MarkAsTaken();
        return dir;
    } else {
        // choose one of the turns
        newDir = emptyDirs[choice - weightStraight];
        nNode[newDir]->MarkAsTaken();
        return newDir;
    }
```

So P(straight) = weightStraight / (weightStraight + numEmptyTurns); weight exactly 100 short-circuits to always-straight; `DIR_NONE` (all blocked) is the stuck/teapot condition. Straight direction counts once with weight; each empty turn neighbor (excluding the straight dir, `GetEmptyTurnNeighbours`, `NODE.CXX:606-619`) counts once. Chosen node is marked taken immediately. (Chase mode uses `ChoosePreferredDirection`, `NODE.CXX:182-228`, picking uniformly among empty preferred dirs toward the lead pipe.)

### Joint selection for JOINT_MIXED and JOINT_CYCLE

Dialog value `ulJointType` (registry, `DIALOG.C:34,83`; enum `JOINT_ELBOW=0, JOINT_BALL, JOINT_MIXED, JOINT_CYCLE` in `DIALOG.H:92-98`) is mapped in the `NORMAL_STATE` constructor, `NSTATE.CXX:29-51`:

```cpp
    bCycleJointStyles = 0;
    switch( ulJointType ) {
        case JOINT_ELBOW:  jointStyle = ELBOWS;  break;
        case JOINT_BALL:   jointStyle = BALLS;   break;
        case JOINT_MIXED:  jointStyle = EITHER;  break;
        case JOINT_CYCLE:
            bCycleJointStyles = 1;
            jointStyle = EITHER;
            break;
    }
```

(`ELBOWS=0, BALLS=1, EITHER=2`, `NUM_JOINT_STYLES 3` — `NSTATE.H:20-28`.)

- **JOINT_MIXED**: `jointStyle` stays `EITHER` forever → every joint runs the `ChooseJointType` EITHER path quoted in (a): 1/1000 teapot, else 1/3 ball (`!ss_iRand(3)`), else elbow.
- **JOINT_CYCLE**: starts at `EITHER`, and on every frame reset `NORMAL_STATE::Reset` (`NSTATE.CXX:158-166`, called from `STATE::FrameReset` at `STATE.CXX:426-428`) advances the style:

```cpp
    // Set the joint style
    if( bCycleJointStyles ) {
        if( ++(jointStyle) >= NUM_JOINT_STYLES )
            jointStyle = 0;
    }
```

so successive frames use EITHER → ELBOWS(0) → BALLS(1) → EITHER(2) → ELBOWS → ... Teapots are possible only on the EITHER frames.

Given a BALL_JOINT/ELBOW_JOINT result, `NORMAL_PIPE::DrawJoint` (`NPIPE.CXX:301-381`) picks which of the 4 notch-variant display lists to call via `ChooseElbow(lastDir,newDir)` (`NPIPE.CXX:280-292`), a lookup in `notchElbDir[oldDir][newDir][4]` (`NPIPE.CXX:235-278`) matched against the running `notchVec`, then updates `notchVec = notchTurn[lastDir][newDir][notchVec]` (`NPIPE.CXX:376`, table in `PIPE.CXX:301-344`).

---

## (d) STATE.CXX — world-full detection, scene reset flow, clearing, timing

### When is the world "full"?

`STATE::Draw` (`STATE.CXX:674-737`) runs every animation tick:
1. For each drawing thread whose pipe `IsStuck()` (`status == PIPE_STUCK`, set when `ChooseNewDirection` returned `DIR_NONE` — `NPIPE.CXX:135-138` draws the end cap first):
   - if `++nPipesDrawn > maxPipesPerFrame` → `pThread->KillPipe()` (frame is saturated) (`STATE.CXX:689-698`);
   - else `pThread->StartPipe()` to begin a new pipe; if `StartPipe()` returns FALSE (pipe got `PIPE_OUT_OF_NODES` because `FindRandomEmptyNode` exhausted the grid — `NODE.CXX:698-740`, `PIPE.CXX:170-197`), then `maxPipesPerFrame = nPipesDrawn;` — i.e. **out of nodes force-ends the frame** (`STATE.CXX:700-706`).
2. Killed threads are compacted out (`STATE.CXX:711-714`).
3. `if( nDrawThreads == 0 ) { resetStatus |= RESET_NORMAL_BIT; return; }` (`STATE.CXX:716-720`) — the frame is over; **the reset itself happens lazily on the next Draw call**: `STATE::DrawValidate` (`STATE.CXX:654-661`) does `if( !resetStatus ) return; FrameReset();` at the top of every `Draw`.

`maxPipesPerFrame` comes from `CalcMaxPipesPerFrame` (`STATE.CXX:561-571`): normal pipes `NORMAL_PIPE_COUNT 5` / textured `NORMAL_TEX_PIPE_COUNT 3` (`NSTATE.H:17-18`), scaled ×1.5 in multi-pipe mode (`STATE.CXX:440`).

### FrameReset flow (`STATE::FrameReset`, STATE.CXX:391-553)

In order:
1. Kill all active pipes (`KillPipe` per thread, `nDrawThreads = 0`) (lines 406-410).
2. **`Clear()`** — clear the screen (line 413; see below).
3. If `RESET_RESIZE_BIT`: `ResetView()` → recompute node array size + `view.SetGLView()` per RC (lines 416-418, 357-378).
4. `nodes->Reset()` — mark every node empty (`STATE.CXX:421`, `NODE.CXX:96-105`).
5. `pNState->Reset()` (joint-style cycling, above) / `pFState->Reset()` + random xRot/zRot for flex (lines 426-433).
6. `maxPipesPerFrame = CalcMaxPipesPerFrame();` and, if multi-pipe, ×1.5 and `nDrawThreads = SS_MIN(maxPipesPerFrame, ss_iRand2(2, maxDrawThreads))` (`MAX_DRAW_THREADS 4`, `STATE.H:20`); 1-in-5 chance of chase mode: `if( bUseChase && (!ss_iRand(5)) ) drawScheme = FRAME_SCHEME_CHASE;` (lines 434-451).
7. Per thread: create/make-current its RC (wglCreateContext + `GLInit()` + `view.SetGLView()` + `wglShareLists(shareRC, ...)` on first use), load modelview = `glLoadIdentity(); glTranslatef(0,0,view.zTrans); glRotatef(view.yRot, 0,1,0);`, create a new `NORMAL_PIPE`/flex pipe, assign chase roles, pick per-thread texture, then `pThread->StartPipe()` which draws the start cap immediately (lines 456-541).
8. `if( resetStatus & RESET_NORMAL_BIT ) view.IncrementSceneRotation();` — the whole scene yaw advances by **9.73156°** per frame-cycle (`STATE.CXX:543-545`, `VIEW.CXX:154-161`).
9. `resetStatus = 0;` (line 548).

### Clearing — SS_DIGITAL_DISSOLVE_CLEAR, not ss_RectWipeClear

`STATE::Clear` (`STATE.CXX:623-642`):

```cpp
    glClear(GL_DEPTH_BUFFER_BIT);

    if( resetStatus & RESET_RESIZE_BIT ) {
        // new window size - recalibrate the transitional clear
        // Calibration is set after a window resize, so window is already black
        ddClear.CalibrateClear( view.winSize.width, view.winSize.height, 2.0f );
    } else if( resetStatus & RESET_NORMAL_BIT )
        // do the normal transitional clear
        ddClear.Clear( view.winSize.width, view.winSize.height );
    else {
        // do a fast one-shot clear
        glClear( GL_COLOR_BUFFER_BIT );
    }
```

- Depth buffer is always cleared with `glClear(GL_DEPTH_BUFFER_BIT)`.
- **Normal end-of-frame reset (`RESET_NORMAL_BIT`)** uses the member `SS_DIGITAL_DISSOLVE_CLEAR ddClear` (`STATE.H:112`), implemented in `COMMON/CLEAR.CXX:207-290`: the screen is divided into `rectSize`-pixel squares and each square is erased in random order with `glScissor(...); glClear(GL_COLOR_BUFFER_BIT); glFlush();` under `GL_SCISSOR_TEST` (collision resolution scans up/down from a random index, CLEAR.CXX:246-285). This is the slow "digital dissolve" wipe you see between pipe scenes.
- **Timing**: on resize/startup (`RESET_RESIZE_BIT`, which includes the very first frame since the STATE ctor sets `resetStatus = RESET_STARTUP_BIT` at `STATE.CXX:47` — startup takes the `else` fast-clear branch; the first *resize* triggers calibration) the dissolve is calibrated to take **2.0 seconds**: `CalibrateClear(w, h, 2.0f)` (`CLEAR.CXX:151-196`) times one dissolve at base square size `min(w,h)/SS_CLEAR_BASE_DIV` (`SS_CLEAR_BASE_DIV 32`, base `rectSize` default `SS_CLEAR_BASE_SIZE 16`, CLEAR.CXX:24-25) and solves `rectSize = sqrt(w*h / idealNRects)` so a full dissolve ≈ `fClearTime` = 2.0 s. (`Clear` also has an unused `static float idealTime = 2.0f;` at CLEAR.CXX:220.)
- Other resets (startup, repaint via `RESET_REPAINT_BIT` from `STATE::Repaint`, `STATE.CXX:328-332`) use a plain one-shot `glClear(GL_COLOR_BUFFER_BIT)`.
- **`ss_RectWipeClear` (CLEAR.CXX:38-113, shrinking-rectangle wipe, declared in `COMMON/SSCOMMON.H:396`) is NOT used by pipes** — no call site anywhere in PIPES (it exists for the common lib / other savers; the GDI variant `ss_GdiRectWipeClear` is compiled out under `#ifdef SS_INITIAL_CLEAR`, CLEAR.CXX:341-407).

### Animation timing context

Pipes runs single-buffered (`ssc.bDoubleBuf = FALSE`, `SSPIPES.CXX:68`) with 16-bit depth (`SS_DEPTH16`, line 69). `STATE::Draw` is invoked as the `ss_UpdateFunc` callback (`SSPIPES.CXX:114`) from the common shell's WM_TIMER-driven update (`SSW::UpdateSSWindow` → `(*UpdateFunc)(DataPtr)`, `COMMON/SSWINDOW.CXX:1097-1108`); the timer is created in `COMMON/SSWPROC.CXX:106-107` `SetTimer(hwnd, idTimer, uiTimeOut, 0)` with `static UINT uiTimeOut = 16; // Cap at ~60 fps` (SSWPROC.CXX:58; debug builds use `2` — "Let it rip!", line 56). Each tick draws **one segment per active pipe** (`STATE.CXX:729-733` loop calling `pThread->DrawPipe()`), which is why pipes grow one node per tick and a frame lasts until all pipes are stuck/killed.

---

## Port-relevant notes (no algorithm changes implied)

- `auxSolidTeapot` is the only glaux dependency in PIPES geometry (`PIPE.CXX:65`); the WASM build must provide an evaluator-based (or pre-tessellated CW-wound) teapot with the same 2.5×radius scale, or the easter egg dies.
- All OBJECTS.CXX geometry is display lists (`glNewList/glCallList`) + `GL_QUAD_STRIP`/`GL_TRIANGLE_FAN` immediate mode — supported by `-sLEGACY_GL_EMULATION=1`.
- The dissolve clear relies on `glScissor` + hundreds of tiny `glClear`s with `glFlush` per rect (CLEAR.CXX:279-281) and wall-clock calibration (`SS_TIMER`), which will need timing shims but no logic changes.
- Multi-pipe mode uses one wgl context per drawing pipe with `wglShareLists` (`STATE.CXX:462-475`); a WebGL port will collapse these to one context (display lists and materials are shared state anyway).
