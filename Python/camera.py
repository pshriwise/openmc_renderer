import math
import numpy as np


class OrbitCamera:
    def __init__(self):
        self.distance = 15.0
        self.azimuth = math.radians(45.0)
        self.elevation = math.radians(30.0)
        self.target = np.array([0.0, 0.0, 0.0], dtype=np.float64)
        self.world_up = np.array([0.0, 0.0, 1.0], dtype=np.float64)
        self.fov = 45.0

        self.rotate_speed = 0.005
        self.pan_speed = 1.0
        self.zoom_speed = 0.1
        self.min_distance = 1.0

    def position(self):
        cos_e = math.cos(self.elevation)
        sin_e = math.sin(self.elevation)
        cos_a = math.cos(self.azimuth)
        sin_a = math.sin(self.azimuth)
        x = self.target[0] + self.distance * cos_e * cos_a
        y = self.target[1] + self.distance * cos_e * sin_a
        z = self.target[2] + self.distance * sin_e
        return np.array([x, y, z], dtype=np.float64)

    def view_vectors(self):
        pos = self.position()
        forward = self.target - pos
        forward = forward / np.linalg.norm(forward)
        right = np.cross(forward, self.world_up)
        right = right / np.linalg.norm(right)
        up = np.cross(right, forward)
        up = up / np.linalg.norm(up)
        return forward, right, up

    def orbit(self, dx, dy):
        self.azimuth += dx * self.rotate_speed
        self.elevation += dy * self.rotate_speed
        max_e = math.radians(89.0)
        self.elevation = max(-max_e, min(max_e, self.elevation))

    def pan(self, dx, dy, viewport_height):
        if viewport_height <= 0:
            return
        _, right, up = self.view_vectors()
        scale = 2.0 * self.distance * math.tan(math.radians(self.fov) * 0.5) / viewport_height
        self.target += (-right * dx + up * dy) * scale * self.pan_speed

    def zoom(self, delta):
        self.distance *= (1.0 - delta * self.zoom_speed)
        if self.distance < self.min_distance:
            self.distance = self.min_distance
