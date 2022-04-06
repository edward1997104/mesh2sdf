# Mesh2SDF

Converts an input mesh to a signed distance field. It can work with arbitrary
meshes, even **non-watertight** meshes from ShapeNet.


## Installation

- Install via the following command:
    ``` shell
    pip install mesh2sdf
    ```

- Alternatively, install from source via the following commands.
    ``` shell
    git clone https://github.com/wang-ps/mesh2sdf.git
    pip install ./mesh2sdf
    ```

## Example

After installing `mesh2sdf`, run the following command to process an input mesh
from ShapeNet:

```shell
python example/test.py
```

![Example of a mesh from ShapeNet](https://raw.githubusercontent.com/wang-ps/mesh2sdf/master/example/data/result.png)


## How does it work?

- Given an input mesh, we first compute the **unsigned** distance field with the
  fast sweeping algorithm implemented by
  [Christopher Batty (SDFGen)](https://github.com/christopherbatty/SDFGen).
  Note that the distance field can always be reliably and accurately computed
  even though the input mesh is non-watertight.

- Then we extract the level sets with a small value **d** with the marching cube
  algorithm. And the extracted level sets are represented with triangle meshes
  and are guaranteed to be manifold.

- There exist multiple connected components in the extracted meshes, and we only
  keep the mesh with the largest bounding box.

- Compute the signed distance field with the remaining triangle mesh as the
  final output. In this way, the signed distance field (SDF) is computed for a
  non-watertight input mesh.
