# Decimation

## Overview

This module is used for performing a topology-preserving reduction of surface triangles.

## Methods

It implements the following methods:

| Method | Description | Supported Format(s) |
|--------|-------------|------------------|
| FastQuadric | Uses [Sven Forstmann's method][Sven-Forstmann] | `obj` |
| Quadric | Uses [vtkQuadricDecimation][vtkQuadricDecimation] based on the work of Garland and Heckbert who first presented the quadric error measure at Siggraph '97 "Surface Simplification Using Quadric Error Metrics" | `obj`, `vtp` |
| DecimatePro | Uses [vtkDecimatePro][vtkDecimatePro] implementing an approach similar to the algorithm originally described in "Decimation of Triangle Meshes", Proc Siggraph `92 | `obj`, `vtp` |

[Sven-Forstmann]: https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
[vtkQuadricDecimation]: https://vtk.org/doc/nightly/html/classvtkQuadricDecimation.html#details
[vtkDecimatePro]: https://vtk.org/doc/nightly/html/classvtkDecimatePro.html#details

Notes:

* Quadric filters provide much better shaped triangles, especially when large reduction ratio is requested.

## Contributors

Authors:
- Beatriz Paniagua (UNC)
- Martin Styner (UNC)
- Hans Johnson (University of Iowa)
- Ben Wilson (Kitware)
- Andras Lasso (PerkLab, Queen's University)

