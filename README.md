# BSP tree
Spatial point location in convex subdivsion using binary space partition tree. Laboratory work for my computational geometry and computer graphics course.

## Prerequisites
Project requires c++11, [CMake] and [CGAL].

## Building
```
cmake && make
```

## Issues
- [ ] [CGAL]'s polyhedron convex decomposition might have intersecting regions, which are not accepted by this algorithm. The folder *data/* contains successfully tested *.off* files, which can be used as input. These files were taken from [CGAL] examples and [Holmes3D files set].

[CGAL]: http://www.cgal.org
[CMake]: http://www.cmake.org
[Holmes3D files set]: http://www.holmes3d.net/graphics/offfiles/
