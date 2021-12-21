# Decimation

## Overview

This module provides an extensible framework for automatic processing of mesh nodes by executing "Tools" on the input to generate output mesh.

Output of a tool can be used as input in another tool, which allows specification of complex editing operations. This is similar to "parametric editing" in engineering CAD software, but this module is specifically developed to work well on complex meshes used in biomedical applications (while most engineering CAD software does not directly support parametric editing of complex polygonal meshes).

## Use cases

* Parcellating white matter surface with curves and planes

* Skull mirroring for facial deformity reconstruction

## Notes

To enable automatic update (so that outputs are automatically recomputed whenever inputs change), check the checkbox on the Apply button.

Tools cannot be run continuously if one of the input nodes is present in the output. The tool can still be run on demand by clicking the apply button.

## Tools

### Append

Combine multiple models into a single output model.

### Boundary cut

Extract a region from the surface that is enclosed by many markup curves and planes. In instances where there is ambiguity about which region should be extracted, a markup fiducial can be used to specify the region of interest.

### Curve cut

Extract a region from the surface that is enclosed by a markup curve.

### Hollow

Create a shell from the surface of the model, effectively making it hollow.

### Margin

Expand or shrink a model by the specified margin.

Notes:
* Requires input models with surface normals precomputed. Normals can be computed using `Surface Toolbox` module or in Python scripting using the `vtkPolyDataNormals` VTK filter.
* May create self-intersecting mesh if the margin value is large or the model has sharp edges.

### Mirror

Reflect the points in a model across the input plane. Useful in conjunction with the plane cut tool to cut a model in half and then mirror the selected half accross the cutting plane.

### SelectByPoints

Allow selecting region(s) of a model node by specifying by markups fiducial points. Model points that are closer to the points than the specified selection distance are selected.

There is an option to save selection in a point scalar and/or clip input model to the selection.
Selection distance can be measured as straight distance or distance on the surface (geodesic distance).

Geodesic distance is computed using code from Krishnan K. "Geodesic Computations on Surfaces." article published in the VTK journal in June 2013 (https://www.vtkjournal.org/browse/publication/891).

### Plane cut

Cut a plane into two separate meshes using any number of markup planes or slice views. The planes can be combined using union, intersection and difference boolean operation.

### ROI cut

Clip a ``vtkMRMLModelNode`` and return the region of the model that is inside or outside the ROI. The tool can also add caps to the clipped regions to maintain a closed surface.


## Contributors

Authors:
- Kyle Sunderland (PerkLab, Queen's University)
- Andras Lasso (PerkLab, Queen's University)
- Mauro I. Dominguez
- Jean-Christophe Fillion-Robin (Kitware)

## Acknowledgements

Development was funded in part by CANARIE’s Research Software Program, OpenAnatomy, and Brigham and Women’s Hospital through NIH grant R01MH112748.
