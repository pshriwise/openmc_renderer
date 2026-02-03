import time
import numpy as np
from PySide6 import QtCore, QtGui, QtWidgets
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import (
    glBindTexture,
    glClear,
    glClearColor,
    glDeleteTextures,
    glDisable,
    glEnable,
    glGenTextures,
    glLoadIdentity,
    glMatrixMode,
    glOrtho,
    glPixelStorei,
    glTexImage2D,
    glTexParameteri,
    glTexSubImage2D,
    glViewport,
    glBegin,
    glEnd,
    glTexCoord2f,
    glVertex2f,
    GL_COLOR_BUFFER_BIT,
    GL_DEPTH_TEST,
    GL_MODELVIEW,
    GL_PROJECTION,
    GL_QUADS,
    GL_RGB,
    GL_TEXTURE_2D,
    GL_TEXTURE_MAG_FILTER,
    GL_TEXTURE_MIN_FILTER,
    GL_LINEAR,
    GL_UNSIGNED_BYTE,
    GL_UNPACK_ALIGNMENT,
)

from camera import OrbitCamera


class GLPlotWidget(QOpenGLWidget):
    def __init__(self, plotter, parent=None):
        super().__init__(parent)
        self._plotter = plotter
        self._camera = OrbitCamera()
        self._texture = None
        self._dirty = True
        self._last_pos = None
        self._buttons = set()
        self._render_mode = "final"
        self._interactive_scale = 0.35
        self._min_frame_interval = 1.0 / 15.0
        self._last_render_time = 0.0
        self._render_size = None

        self._idle_timer = QtCore.QTimer(self)
        self._idle_timer.setSingleShot(True)
        self._idle_timer.timeout.connect(self._request_final_render)

        self._throttle_timer = QtCore.QTimer(self)
        self._throttle_timer.setSingleShot(True)
        self._throttle_timer.timeout.connect(self._do_update)

        self.setFocusPolicy(QtCore.Qt.StrongFocus)

    def minimumSizeHint(self):
        return QtCore.QSize(400, 300)

    def sizeHint(self):
        return QtCore.QSize(900, 700)

    def initializeGL(self):
        glClearColor(0.05, 0.05, 0.06, 1.0)
        glDisable(GL_DEPTH_TEST)
        glEnable(GL_TEXTURE_2D)

        self._texture = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, self._texture)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        self._dirty = True

    def resizeGL(self, width, height):
        glViewport(0, 0, width, height)
        if width > 0 and height > 0:
            self._render_mode = "interactive"
            self._dirty = True
            self._idle_timer.start(250)
            self._request_redraw()

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT)

        if self._dirty:
            self._sync_plotter_camera()
            width, height = self._current_render_size()
            self._ensure_plotter_size(width, height)
            image = self._plotter.create_image()
            self._upload_texture(image)
            self._dirty = False
            self._last_render_time = time.monotonic()

        self._draw_textured_quad()

    def _sync_plotter_camera(self):
        pos = self._camera.position()
        _, _, up = self._camera.view_vectors()
        self._plotter.set_camera(
            position=pos,
            look_at=self._camera.target,
            up=up,
            fov=self._camera.fov,
            light_position=pos,
        )

    def _upload_texture(self, image):
        if self._texture is None:
            return
        image = np.ascontiguousarray(image, dtype=np.uint8)
        height, width, _ = image.shape
        glBindTexture(GL_TEXTURE_2D, self._texture)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            width,
            height,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            image,
        )

    def _current_render_size(self):
        width = max(1, self.width())
        height = max(1, self.height())
        if self._render_mode == "interactive":
            width = max(64, int(width * self._interactive_scale))
            height = max(64, int(height * self._interactive_scale))
        return width, height

    def _ensure_plotter_size(self, width, height):
        if self._render_size != (width, height):
            self._plotter.set_pixels(width, height)
            self._render_size = (width, height)

    def _do_update(self):
        self.update()

    def _request_redraw(self, force=False):
        if force:
            self.update()
            return
        now = time.monotonic()
        elapsed = now - self._last_render_time
        if elapsed >= self._min_frame_interval and not self._throttle_timer.isActive():
            self.update()
            return
        if not self._throttle_timer.isActive():
            delay = max(0.0, self._min_frame_interval - elapsed)
            self._throttle_timer.start(int(delay * 1000))

    def _request_interactive_render(self):
        self._render_mode = "interactive"
        self._dirty = True
        self._idle_timer.start(250)
        self._request_redraw()

    def _request_final_render(self):
        self._render_mode = "final"
        self._dirty = True
        self._request_redraw(force=True)

    def request_final_render(self):
        self._request_final_render()

    def _draw_textured_quad(self):
        if self._texture is None:
            return
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        glOrtho(0, 1, 0, 1, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

        glBindTexture(GL_TEXTURE_2D, self._texture)
        glBegin(GL_QUADS)
        glTexCoord2f(0.0, 0.0)
        glVertex2f(0.0, 0.0)
        glTexCoord2f(1.0, 0.0)
        glVertex2f(1.0, 0.0)
        glTexCoord2f(1.0, 1.0)
        glVertex2f(1.0, 1.0)
        glTexCoord2f(0.0, 1.0)
        glVertex2f(0.0, 1.0)
        glEnd()

    def mousePressEvent(self, event):
        self._last_pos = event.position()
        self._buttons.add(event.button())
        event.accept()

    def mouseReleaseEvent(self, event):
        if event.button() in self._buttons:
            self._buttons.remove(event.button())
        if not self._buttons:
            self._request_final_render()
        event.accept()

    def mouseMoveEvent(self, event):
        if self._last_pos is None:
            self._last_pos = event.position()
            return

        dx = event.position().x() - self._last_pos.x()
        dy = event.position().y() - self._last_pos.y()

        if QtCore.Qt.LeftButton in self._buttons:
            self._camera.orbit(dx, dy)
            self._request_interactive_render()
        elif QtCore.Qt.RightButton in self._buttons:
            self._camera.pan(dx, dy, self.height())
            self._request_interactive_render()

        self._last_pos = event.position()

    def wheelEvent(self, event):
        delta = event.angleDelta().y() / 120.0
        self._camera.zoom(delta)
        self._request_interactive_render()

    def closeEvent(self, event):
        if self._texture is not None:
            glDeleteTextures(1, [self._texture])
            self._texture = None
        super().closeEvent(event)
