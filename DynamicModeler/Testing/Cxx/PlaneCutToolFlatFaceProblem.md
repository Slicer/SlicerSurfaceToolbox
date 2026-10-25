# Plane cut: planes that lie on a flat face of the model

Known problem of the capped outputs of the Dynamic Modeler plane cut tool
(`vtkSlicerDynamicModelerPlaneCutTool`), introduced by the `vtkClipClosedSurface` based capping
(commits 06e74e6 and 1be80d2). The old algorithm (c96cdbe) did not have it.

`TestPlaneOnFlatFace` in `vtkSlicerDynamicModelerPlaneCutToolRegressionTest.cxx` reproduces it. Its result is
currently ignored (the function always returns `true`). Once this is fixed, make it return `success` again.

## Symptom

When a cutting plane contains a flat face of the model, for example a box cut by the plane of one of its faces:

- the output that should contain the whole model loses part or all of that face, so it is open and its volume is too small;
- the output that should be empty contains a zero-thickness leftover sheet, sometimes with non-manifold edges
  and a nonzero (negative) volume.

Results of the test for a box from (-10, -10, -10) to (10, 10, 10) made with `vtkTessellatedBoxSource` and
triangulated (level `n` gives `n + 1` subdivisions per edge), cut with a markups plane at x = 10 or x = -10,
normal (1, 0, 0). The expected volumes are 8000 and 0:

| Level | Plane x = 10: negative output volume | Plane x = -10: negative output volume |
|---|---|---|
| 1 | 8000 | 0 |
| 2 | 7555.56 (open) | -444.444 |
| 3 | 8000 | 0 |
| 4 | 7733.33 (open) | -266.667 |
| 5 | 7555.56 (open) | -444.444 |

Levels 1 and 3 pass the volume checks, but the side that should be empty still gets a zero-volume sheet
(for level 3: 26 triangles with 2 non-manifold edges). Levels 6 to 8 fail like levels 2, 4 and 5.

Cutting an output of the tool again with the same plane (the cap of the first cut lies on the plane)
gives the correct result in the test.

## Cause

There are two parts.

**1. `vtkClipClosedSurface` does not keep faces that lie on a clipping plane.** It replaces them by new cut faces,
labeled 1 in the `Labels` cell array when `ScalarModeToLabels` is on. For the level 4 box (300 triangles,
50 of them on the x = 10 face), with a plane at x = 10 and a normal of exactly (1, 0, 0):

| Kept side | Original cells (label 0) | Cut faces (label 1) |
|---|---|---|
| x > 10 | 0 | 0 |
| x < 10 | 250 | 18 |

The face is removed from the x < 10 side and re-created as 18 cut faces.

**2. The complement output only uses the cut faces of the intersection clipper.** `CreateCappedOutputs` builds the
complement from the original cells (label 0) of the complement piece clippers, plus the reversed cap of the
intersection clipper. This keeps the seams conforming, but it assumes that the cap of the intersection is also the cap
of the complement. When the face lies on the plane, the complement piece re-creates the face as label 1 cells,
and those are discarded. The intersection side is empty, so its cap is empty too, and the whole face goes missing.
With a slice node as the plane (normal exactly (1, 0, 0)), the full side loses the whole face: the level 4 box gives
250 triangles and 20 boundary edges.

**Rounding of markups normals changes the failure mode.** `vtkMRMLMarkupsPlaneNode::SetNormal(1, 0, 0)` gives back
`GetNormalWorld()` = (1, 0, 2.2204460492503131e-16). The vertices of the face are then not exactly on the plane: they
are on the positive or negative side by about 1e-15 depending on their z coordinate (18 positive and 18 negative
for level 4). `vtkClipClosedSurface` then keeps the face triangles that have no vertex on the negative side as
original cells of the x > 10 side: 20 triangles, which form the zero-thickness sheet. The cut faces of the two sides
no longer cover the same region, which gives the band-shaped hole on the full side. The hole is along the row of face
vertices just below z = 0. Snapping the normal to the axis would therefore not fix the problem, it would only give the
"whole face missing" result above.

## What the old algorithm did

The old algorithm clipped the surface with `vtkClipPolyData`, which keeps cells with all point values at 0
(on the plane) on one side. It made the cap separately with `vtkCutter` and `vtkContourTriangulator`. The full side
kept the whole face (volume 8000). The empty side got a zero-volume sheet from the degenerate cap, and its seams were
not stitched to the body.

## Fix that was tried

Before clipping, drop every plane that does not cut the surface, that is, when the whole surface is on one side of
the plane within the clipper tolerance (1e-6):

- whole surface on the kept side of the plane: the plane does not change the intersection, so do not clip with it;
- whole surface on the removed side: the intersection is empty and the complement is the whole surface
  (`vtkClipClosedSurface` without planes passes the surface through).

```cpp
// Returns false if the whole surface is on the removed side of a plane, so that the intersection is empty.
bool GetCuttingPlanes(vtkPlaneCollection* intersectionPlanes, vtkPoints* points, double tolerance,
  vtkPlaneCollection* cuttingPlanes)
{
  for (int planeIndex = 0; planeIndex < intersectionPlanes->GetNumberOfItems(); ++planeIndex)
    {
    vtkPlane* plane = intersectionPlanes->GetItem(planeIndex);
    double normal[3] = { 0.0 };
    plane->GetNormal(normal);
    double normalLength = vtkMath::Norm(normal);
    double minimumDistance = VTK_DOUBLE_MAX;
    double maximumDistance = -VTK_DOUBLE_MAX;
    for (vtkIdType pointIndex = 0; pointIndex < points->GetNumberOfPoints(); ++pointIndex)
      {
      double distance = plane->EvaluateFunction(points->GetPoint(pointIndex)) / normalLength;
      minimumDistance = std::min(minimumDistance, distance);
      maximumDistance = std::max(maximumDistance, distance);
      }
    if (minimumDistance >= -tolerance)
      {
      continue; // The whole surface is on the kept side of the plane.
      }
    if (maximumDistance <= tolerance)
      {
      return false; // The whole surface is on the removed side of the plane.
      }
    cuttingPlanes->AddItem(plane);
    }
  return true;
}
```

`CreateCappedOutputs` then clipped with the cutting planes only, built the complement only if there was at least one
cutting plane, and swapped the intersection and complement outputs when the intersection was empty.

Result: `TestPlaneOnFlatFace` passed for all levels and the rest of the regression test still passed.
`vtkSlicerDynamicModelerPlaneCutToolClosedSurfaceTest` was unchanged (same 5 of 8 cases passing, same failure output),
and the timing did not change.

Limitation: it only handles planes that do not cut the model, such as a face on the outside of the model or a plane
touching it. A plane that contains a face and also cuts the model elsewhere still fails. This happens, for example,
with segmentation surfaces without smoothing, where many faces are aligned with the voxel grid (the SegmentationSurface
case of `vtkSlicerDynamicModelerPlaneCutToolClosedSurfaceTest`). The change was reverted to fix the problem in general
later.

## Directions for a general fix

- Assign faces that lie on a plane (all vertices within the tolerance) to a side explicitly: the side opposite to
  their outward normal, which is the side of the volume they bound. Remove them from the input to the clippers,
  then add them back to that side's output, stitched to the cut faces.
- Build the cap of the complement from the cut faces (label 1) of the complement pieces instead of reusing the
  reversed cap of the intersection. This would need a different way of making the seams between the pieces and the
  cap conform.
- Classify points with a tolerance (|f| < tolerance counts as on the plane) consistently in every clipper, so that
  rounding errors in the plane normal or the point coordinates cannot split a face between the sides.
