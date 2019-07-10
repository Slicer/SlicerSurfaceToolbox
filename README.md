# SurfaceToolbox

This repository contains SlicerSurfaceToolbox extension for 3D Slicer, which consists of modules for 
supporting cleanup and optimization processes on surface models.

The associated modules are integrated into Slicer as [remote modules](https://www.slicer.org/wiki/Documentation/Nightly/Developers/Build_system/Remote_Module) and distributed within the official Slicer package available at https://download.slicer.org.


## Contact

Questions regarding this extension should be posted on 3D Slicer forum: https://discourse.slicer.org


## History

The original SurfaceToolbox python module was created by Lucas Antiga (Orobix) in 2013, integrated into Slicer
by Steve Pieper (BWH), and maintained by the Slicer community in the main Slicer source repository.

In June 2019, Bea Paniagua (Kitware) shared on the Slicer forum [plans][plans] to revamp the
[MeshMath][MeshMath] functionalities into the SurfaceToolbox. Ben Wilson (Kitware Summer intern 2019)
with input from Jean-Christophe Fillion-Robin (Kitware) and Andras Lasso (Queens) started to work on
improving and modularizing the project.

In July 2019, this GitHub project was created by extracting the history from the main Slicer repository.

[plans]: https://discourse.slicer.org/t/surface-toolbox-revamp/7113
[MeshMath]: https://github.com/NIRALUser/SPHARM-PDM/blob/4baa99c10c58bc399faf931547f42c3d31469085/Modules/CLI/MetaMeshTools/MeshMath.cxx


## License

This project is distributed under the BSD-style Slicer license allowing academic and commercial use without any restrictions. Please see the [LICENSE](LICENSE) file for details.
