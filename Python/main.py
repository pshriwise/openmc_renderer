import sys
from pathlib import Path

from PySide6 import QtCore, QtGui, QtWidgets
from PySide6.QtGui import QSurfaceFormat

# Ensure local OpenMC Python package can be imported without installation.
REPO_ROOT = Path(__file__).resolve().parents[1]
OPENMC_PY_ROOT = REPO_ROOT / "openmc"
if str(OPENMC_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(OPENMC_PY_ROOT))

from openmc_plotter import OpenMCPlotter
from gl_widget import GLPlotWidget


def split_args(argv):
    if "--" in argv:
        idx = argv.index("--")
        return argv[:idx], argv[idx + 1:]
    return argv, []


def main():
    qt_args, openmc_args = split_args(sys.argv[1:])

    fmt = QSurfaceFormat()
    fmt.setVersion(2, 1)
    fmt.setProfile(QSurfaceFormat.CompatibilityProfile)
    QSurfaceFormat.setDefaultFormat(fmt)

    app = QtWidgets.QApplication([sys.argv[0]] + qt_args)

    plotter = OpenMCPlotter(args=openmc_args)

    window = QtWidgets.QMainWindow()
    window.setWindowTitle("OpenMC Renderer (PySide6)")

    gl_widget = GLPlotWidget(plotter)
    window.setCentralWidget(gl_widget)

    materials_dock = QtWidgets.QDockWidget("Materials", window)
    materials_dock.setAllowedAreas(
        QtCore.Qt.LeftDockWidgetArea | QtCore.Qt.RightDockWidgetArea
    )

    materials_panel = QtWidgets.QWidget(materials_dock)
    materials_layout = QtWidgets.QVBoxLayout(materials_panel)
    materials_layout.setContentsMargins(8, 8, 8, 8)

    scroll_area = QtWidgets.QScrollArea(materials_panel)
    scroll_area.setWidgetResizable(True)
    scroll_container = QtWidgets.QWidget(scroll_area)
    scroll_layout = QtWidgets.QVBoxLayout(scroll_container)
    scroll_layout.setAlignment(QtCore.Qt.AlignTop)

    if plotter.available:
        for mat_id, name in plotter.material_list():
            label = f"{mat_id}"
            if name:
                label = f"{mat_id} - {name}"
            checkbox = QtWidgets.QCheckBox(label, scroll_container)
            checkbox.setChecked(True)

            def _make_toggle(mid):
                return lambda checked: _on_material_toggle(mid, checked, plotter, gl_widget)

            checkbox.toggled.connect(_make_toggle(mat_id))
            scroll_layout.addWidget(checkbox)
    else:
        scroll_layout.addWidget(
            QtWidgets.QLabel("OpenMC not available.", scroll_container)
        )

    scroll_container.setLayout(scroll_layout)
    scroll_area.setWidget(scroll_container)
    materials_layout.addWidget(scroll_area)
    materials_panel.setLayout(materials_layout)
    materials_dock.setWidget(materials_panel)
    window.addDockWidget(QtCore.Qt.RightDockWidgetArea, materials_dock)

    if not plotter.available:
        err = plotter.import_error
        msg = "OpenMC library not available; showing fallback image."
        if err is not None:
            msg += f"\n{err}"
        QtWidgets.QMessageBox.warning(window, "OpenMC", msg)

    window.resize(900, 700)
    window.show()

    exit_code = app.exec()
    plotter.finalize()
    return exit_code


def _on_material_toggle(material_id, checked, plotter, gl_widget):
    plotter.set_material_visibility(material_id, checked)
    gl_widget.request_final_render()


if __name__ == "__main__":
    raise SystemExit(main())
