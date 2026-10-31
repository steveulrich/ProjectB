"""Fit a zero-roll Unreal camera to recorded world/pixel correspondences.

Uses finite-difference damped least squares with NumPy. It only writes a JSON
suggestion, never changes the central configuration or editor cameras.
"""
import argparse
import json
import math
from pathlib import Path
import numpy as np


def solve(path, output):
    data = json.loads(path.read_text())
    points = np.asarray([p['world_cm'] for p in data['landmarks']], dtype=float)
    desired = np.asarray([p['reference_pixel'] for p in data['landmarks']], dtype=float)
    width, height = data['image_width'], data['image_height']
    x = np.r_[data['initial_camera_cm'], np.radians(data['initial_yaw_pitch_degrees']),
              math.radians(data['initial_horizontal_fov_degrees'])]

    def basis(value):
        yaw, pitch = value[3:5]
        forward = np.array([math.cos(pitch)*math.cos(yaw), math.cos(pitch)*math.sin(yaw), math.sin(pitch)])
        right = np.array([-math.sin(yaw), math.cos(yaw), 0])
        return forward, right, np.cross(forward, right)

    def predict(value):
        forward, right, up = basis(value)
        delta = points-value[:3]
        depth = delta@forward
        lens = width/2/math.tan(value[5]/2)
        return np.c_[width/2+lens*(delta@right)/depth, height/2-lens*(delta@up)/depth]

    def residual(value):
        return (predict(value)-desired).ravel()

    damping = 0.001
    for iteration in range(200):
        error = residual(x)
        steps = [1, 1, 1, 0.0001, 0.0001, 0.0001]
        jacobian = np.column_stack([(residual(x+np.eye(6)[i]*steps[i])-error)/steps[i] for i in range(6)])
        jt_j = jacobian.T@jacobian
        metric = np.diag(np.maximum(np.diag(jt_j), 1e-9))
        delta = np.linalg.solve(jt_j+damping*metric, -jacobian.T@error)
        candidate = x+delta
        if 20 < math.degrees(candidate[5]) < 140 and np.linalg.norm(residual(candidate)) < np.linalg.norm(error):
            x = candidate
            damping = max(1e-12, damping/2)
        else:
            damping = min(damping*4, 1e12)
        if np.linalg.norm(delta) < 1e-7:
            break
    forward, _, _ = basis(x)
    result = {'status': 'suggested_camera_only; requires Unreal visual comparison', 'input': str(path),
              'location_cm': x[:3].tolist(), 'rotation_pitch_yaw_roll_degrees': [math.degrees(x[4]), math.degrees(x[3]), 0],
              'horizontal_fov_degrees': math.degrees(x[5]), 'look_at_cm': (x[:3]+forward*6000).tolist(),
              'rms_pixel_error': float(np.sqrt(np.mean(residual(x)**2))), 'predicted_pixels': predict(x).tolist(),
              'landmarks': data['landmarks'], 'assumptions': data['assumptions']}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('location_cm', 'rotation_pitch_yaw_roll_degrees', 'horizontal_fov_degrees', 'rms_pixel_error')}))


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument('--landmarks', type=Path, default=root/'Config/camera_ref01_landmarks.json')
    parser.add_argument('--output', type=Path, default=root.parents[1]/'Saved/Logs/dorado-reconstruction-20261030/camera-ref01-fit-four-landmarks.json')
    args = parser.parse_args()
    solve(args.landmarks, args.output)
