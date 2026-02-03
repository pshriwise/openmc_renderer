import numpy as np

OPENMC_IMPORT_ERROR = None
try:
    import openmc.lib as omlib
    from openmc.lib import plot as plotlib
    OPENMC_AVAILABLE = True
except Exception as exc:  # pragma: no cover - fallback for missing openmc/lib
    OPENMC_AVAILABLE = False
    OPENMC_IMPORT_ERROR = exc
    omlib = None
    plotlib = None


class OpenMCPlotter:
    def __init__(self, args=None, width=800, height=600):
        self._width = int(width)
        self._height = int(height)
        self._available = OPENMC_AVAILABLE
        self._plot = None

        if self._available:
            if not omlib.is_initialized:
                omlib.init(args=args or [], output=True)
            self._plot = plotlib.PhongPlot()
            self._plot.set_color_by(plotlib.PhongPlot.COLOR_BY_MATERIAL)
            self._plot.set_pixels(self._width, self._height)
            self._plot.set_default_colors()
            self._plot.set_all_opaque()

            # Default camera
            self.set_camera(
                position=(10.0, 10.0, 10.0),
                look_at=(0.0, 0.0, 0.0),
                up=(0.0, 0.0, 1.0),
                fov=45.0,
                light_position=(10.0, 10.0, 10.0),
            )

    @property
    def available(self):
        return self._available

    @property
    def import_error(self):
        return OPENMC_IMPORT_ERROR

    def set_pixels(self, width, height):
        self._width = int(width)
        self._height = int(height)
        if self._plot is not None:
            self._plot.set_pixels(self._width, self._height)

    def set_camera(self, position, look_at, up, fov, light_position=None):
        if self._plot is None:
            return
        self._plot.set_camera_position(*position)
        self._plot.set_look_at(*look_at)
        self._plot.set_up(*up)
        self._plot.set_fov(float(fov))
        if light_position is None:
            light_position = position
        self._plot.set_light_position(*light_position)

    def material_list(self):
        if not self._available:
            return []
        mats = []
        for mat_id in omlib.materials:
            mat = omlib.materials[mat_id]
            name = mat.name
            label = name if name else ""
            mats.append((mat_id, label))
        mats.sort(key=lambda item: item[0])
        return mats

    def set_material_visibility(self, material_id, visible):
        if self._plot is None:
            return
        self._plot.set_visibility(int(material_id), bool(visible))

    def create_image(self):
        if self._plot is None:
            return self._fallback_image()
        return self._plot.create_image()

    def finalize(self):
        if self._available and omlib.is_initialized:
            omlib.finalize()

    def _fallback_image(self):
        # Simple checkerboard to confirm rendering path when OpenMC isn't available.
        tile = 32
        h, w = self._height, self._width
        y = np.arange(h)[:, None]
        x = np.arange(w)[None, :]
        checker = ((x // tile) + (y // tile)) % 2
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[checker == 0] = (40, 40, 40)
        img[checker == 1] = (80, 80, 80)
        return img
