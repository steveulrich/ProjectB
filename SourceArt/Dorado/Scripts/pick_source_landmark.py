"""Pick a recovered LOD0 triangle using a recorded render's camera and pixel.

Runs outside Unreal. This is a manual image-correspondence aid, not a renderer
or an acceptance check. The supported source mesh has an identity root node.
"""
import argparse
import json
import math
from pathlib import Path
import numpy as np
from analyze_ring_gaps import parse_mesh
from audit_source_scope import local_matrix


def pick(manifest, camera, asset_fragment, pixels, width, height):
    yaw, pitch = np.radians(camera['rotation_pitch_yaw_roll_degrees'][1::-1])
    forward = np.array([math.cos(pitch)*math.cos(yaw), math.cos(pitch)*math.sin(yaw), math.sin(pitch)])
    right = np.array([-math.sin(yaw), math.cos(yaw), 0.0])
    up = np.cross(forward, right)
    origin = np.asarray(camera['location_cm'])
    lens = width/2/math.tan(math.radians(camera['horizontal_fov_degrees'])/2)
    center = np.asarray(manifest['source_center_m'])
    meshes = []
    for row in manifest['instances']:
        if asset_fragment not in row['asset_id']:
            continue
        vertices, faces = parse_mesh(row['asset_id'].removeprefix('SM_Dorado_'), exclude_proxy=True)
        matrix = np.array(local_matrix({'Pos': ','.join(map(str, row['source_position_m'])),
                                        'Rotate': ','.join(map(str, row['source_rotation_wxyz'])),
                                        'Scale': ','.join(map(str, row['source_scale']))}))
        points = (np.c_[vertices, np.ones(len(vertices))]@matrix.T)[:, :3]
        points = (points-center)*[50, -50, 50]
        meshes.append((row['name'], points[np.asarray(faces)]))
    output = []
    for pixel in pixels:
        direction = forward+right*((pixel[0]-width/2)/lens)+up*((height/2-pixel[1])/lens)
        direction /= np.linalg.norm(direction)
        hits = []
        for name, tri in meshes:
            edge1, edge2 = tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]
            cross = np.cross(direction, edge2)
            determinant = np.einsum('ij,ij->i', edge1, cross)
            valid = np.abs(determinant)>1e-8
            inv = np.divide(1, determinant, out=np.zeros_like(determinant), where=valid)
            offset = origin-tri[:, 0]
            u = np.einsum('ij,ij->i', offset, cross)*inv
            q = np.cross(offset, edge1)
            v = q@direction*inv
            t = np.einsum('ij,ij->i', edge2, q)*inv
            good = np.flatnonzero(valid & (u>=0) & (v>=0) & (u+v<=1) & (t>0))
            if len(good):
                best = good[np.argmin(t[good])]
                hits.append({'instance': name, 'distance_cm': float(t[best]),
                             'world_cm': (origin+direction*t[best]).tolist()})
        output.append({'pixel': pixel, 'hits': sorted(hits, key=lambda h: h['distance_cm'])})
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--camera', type=Path, required=True)
    parser.add_argument('--asset', required=True)
    parser.add_argument('--pixels', required=True, help='JSON array of [x,y] pairs')
    parser.add_argument('--width', type=int, default=2048)
    parser.add_argument('--height', type=int, default=918)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    result = pick(json.loads((root/'geometry_manifest.json').read_text()),
                  json.loads(args.camera.read_text()), args.asset,
                  json.loads(args.pixels), args.width, args.height)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
