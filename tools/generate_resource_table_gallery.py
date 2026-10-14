# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

"""生成 Tutorial 12 使用的可控多材质 glTF 资产。"""

import math
import os
import sys

import bpy


DEFAULT_COUNT = 16
GRID_COLUMNS = 4
IMAGE_SIZE = 32


def clear_scene() -> None:
  bpy.ops.object.select_all(action="SELECT")
  bpy.ops.object.delete(use_global=False)
  for datablocks in (bpy.data.materials, bpy.data.images, bpy.data.meshes, bpy.data.curves):
    for datablock in list(datablocks):
      datablocks.remove(datablock)


def make_image(name: str, pixels: list[float], colorspace: str) -> bpy.types.Image:
  image = bpy.data.images.new(name, width=IMAGE_SIZE, height=IMAGE_SIZE, alpha=False)
  image.colorspace_settings.name = colorspace
  image.pixels = pixels if len(pixels) != 4 else pixels * (IMAGE_SIZE * IMAGE_SIZE)
  return image


def make_material(index: int, count: int) -> bpy.types.Material:
  hue = index / float(max(count, 1))
  red = 0.25 + 0.7 * abs(math.sin(hue * math.pi))
  green = 0.25 + 0.7 * abs(math.sin((hue + 0.33) * math.pi))
  blue = 0.25 + 0.7 * abs(math.sin((hue + 0.66) * math.pi))
  roughness = 0.15 + 0.75 * ((index * 7) % 16) / 15.0
  metallic = ((index * 5) % 11) / 10.0

  base_pixels = []
  mask_pixels = []
  for y in range(IMAGE_SIZE):
    for x in range(IMAGE_SIZE):
      checker = 0.72 if (x // 8 + y // 8) % 2 else 1.0
      base_pixels.extend([red * checker, green * checker, blue * checker, 1.0])
      row_factor = 0.8 + 0.2 * y / max(IMAGE_SIZE - 1, 1)
      mask_pixels.extend([metallic, roughness * row_factor, 0.0, 1.0])
  base_image = make_image(f"Gallery_BaseColor_{index:02d}", base_pixels, "sRGB")
  mask_image = make_image(f"Gallery_MetallicRoughness_{index:02d}", mask_pixels, "Non-Color")

  material = bpy.data.materials.new(f"GalleryMaterial_{index:02d}")
  material.use_nodes = True
  nodes = material.node_tree.nodes
  links = material.node_tree.links
  nodes.clear()
  output = nodes.new("ShaderNodeOutputMaterial")
  shader = nodes.new("ShaderNodeBsdfPrincipled")
  base = nodes.new("ShaderNodeTexImage")
  base.image = base_image
  base.label = "BaseColor"
  mask = nodes.new("ShaderNodeTexImage")
  mask.image = mask_image
  mask.label = "MetallicRoughness"
  separate = nodes.new("ShaderNodeSeparateColor")
  separate.mode = "RGB"
  links.new(base.outputs["Color"], shader.inputs["Base Color"])
  links.new(mask.outputs["Color"], shader.inputs["Metallic"])
  links.new(mask.outputs["Color"], separate.inputs["Color"])
  links.new(separate.outputs["Green"], shader.inputs["Roughness"])
  links.new(shader.outputs["BSDF"], output.inputs["Surface"])
  return material


def add_shape(index: int, material: bpy.types.Material, columns: int) -> None:
  x = (index % columns) * 2.4 - (columns - 1) * 1.2
  y = (index // columns) * 2.4 - 3.6
  shape = index % 4
  if shape == 0:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, location=(x, y, 0.65))
  elif shape == 1:
    bpy.ops.mesh.primitive_cube_add(location=(x, y, 0.65))
    bpy.context.object.scale = (0.65, 0.65, 0.65)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
  elif shape == 2:
    bpy.ops.mesh.primitive_torus_add(major_segments=24, minor_segments=10, location=(x, y, 0.65))
  else:
    bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=0.75, radius2=0.25, depth=1.3,
                                    location=(x, y, 0.65))
  object_ = bpy.context.object
  object_.name = f"GalleryObject_{index:02d}"
  object_.data.name = f"GalleryMesh_{shape:02d}"
  object_.data.materials.append(material)
  for polygon in object_.data.polygons:
    polygon.use_smooth = shape != 1


def main() -> None:
  arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
  if not arguments:
    raise RuntimeError("需要输出目录参数")
  output_dir = os.path.abspath(arguments[-1])
  count = DEFAULT_COUNT
  if "--count" in arguments:
    count_index = arguments.index("--count") + 1
    if count_index >= len(arguments) - 1:
      raise RuntimeError("--count 需要一个位于输出目录之前的整数")
    count = int(arguments[count_index])
  if count <= 0:
    raise RuntimeError("材质数量必须大于 0")
  columns = GRID_COLUMNS
  os.makedirs(output_dir, exist_ok=True)
  clear_scene()
  for index in range(count):
    add_shape(index, make_material(index, count), columns)

  bpy.ops.object.select_all(action="SELECT")
  bpy.ops.export_scene.gltf(
      filepath=os.path.join(output_dir, "ResourceTableGallery.gltf"),
      export_format="GLTF_SEPARATE",
      export_image_format="AUTO",
      export_texture_dir="textures",
      export_apply=True,
      export_cameras=False,
      export_lights=False,
  )


if __name__ == "__main__":
  main()
