"""Read a loaded Blender source scene without changing or saving it.

blender -b SOURCE.blend --python inspect_source.py -- OUTPUT.json
"""
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def values(sequence):
    return [round(float(value), 5) for value in sequence]


def inspect():
    output = Path(sys.argv[sys.argv.index("--") + 1])
    objects = []
    for obj in bpy.data.objects:
        record = {
            "name": obj.name, "type": obj.type,
            "location": values(obj.location), "rotation_euler": values(obj.rotation_euler),
            "scale": values(obj.scale), "dimensions": values(obj.dimensions),
            "matrix_world": [values(row) for row in obj.matrix_world],
            "collections": [collection.name for collection in obj.users_collection],
        }
        if obj.type == "MESH":
            corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
            record.update({
                "vertices": len(obj.data.vertices), "polygons": len(obj.data.polygons),
                "triangles": sum(max(0, len(poly.vertices) - 2) for poly in obj.data.polygons),
                "uv_layers": [layer.name for layer in obj.data.uv_layers],
                "materials": [slot.material.name if slot.material else None for slot in obj.material_slots],
                "bounds_min": values(min(v[axis] for v in corners) for axis in range(3)),
                "bounds_max": values(max(v[axis] for v in corners) for axis in range(3)),
            })
        objects.append(record)
    images = []
    for image in bpy.data.images:
        resolved = bpy.path.abspath(image.filepath)
        images.append({"name": image.name, "path": image.filepath, "resolved_path": resolved,
                       "packed": bool(image.packed_file), "exists": Path(resolved).is_file(),
                       "size": list(image.size), "color_space": image.colorspace_settings.name})
    materials = []
    for material in bpy.data.materials:
        materials.append({"name": material.name, "diffuse_color": values(material.diffuse_color),
                          "use_nodes": material.use_nodes,
                          "images": [node.image.name for node in material.node_tree.nodes
                                     if node.type == "TEX_IMAGE" and node.image] if material.node_tree else []})
    report = {"blender_version": bpy.app.version_string, "source": bpy.data.filepath,
              "scene_unit_system": bpy.context.scene.unit_settings.system,
              "scene_unit_scale": bpy.context.scene.unit_settings.scale_length,
              "objects": objects, "materials": materials, "images": images,
              "summary": {"objects": len(objects), "meshes": sum(o["type"] == "MESH" for o in objects),
                          "triangles": sum(o.get("triangles", 0) for o in objects),
                          "materials": len(materials), "images": len(images),
                          "missing_images": sum(not i["exists"] and not i["packed"] for i in images)}}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("DORADO_SOURCE_INVENTORY=" + json.dumps(report["summary"]))


if __name__ == "__main__":
    inspect()
