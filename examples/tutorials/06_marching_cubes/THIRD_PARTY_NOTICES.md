# Marching Cubes 查表来源

`marching_cubes_table.h` 的 256 个 Hexahedron case 固定取自 VTK 提交
`6849c276a2b7932263044a0a7bcea533fcb71943` 的
`Common/DataModel/vtkMarchingCellsContourCases.cxx`，并展平为 C++ 定宽数组。

原始表版权归 Ken Martin、Will Schroeder 与 Bill Lorensen 所有，使用 BSD-3-Clause 许可证。
VTK 项目：https://github.com/Kitware/VTK
