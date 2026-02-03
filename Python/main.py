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

    help_action = QtGui.QAction("Controls", window)
    help_action.setShortcuts([QtGui.QKeySequence.HelpContents, QtGui.QKeySequence("Shift+/")])

    help_menu = window.menuBar().addMenu("&Help")
    help_menu.addAction(help_action)

    gl_widget = GLPlotWidget(plotter)
    help_action.triggered.connect(gl_widget.toggle_help_overlay)
    window.setCentralWidget(gl_widget)

    materials_dock = QtWidgets.QDockWidget("Visibility", window)
    materials_dock.setAllowedAreas(
        QtCore.Qt.LeftDockWidgetArea | QtCore.Qt.RightDockWidgetArea
    )

    materials_panel = QtWidgets.QWidget(materials_dock)
    materials_layout = QtWidgets.QVBoxLayout(materials_panel)
    materials_layout.setContentsMargins(8, 8, 8, 8)

    mode_layout = QtWidgets.QHBoxLayout()
    mode_label = QtWidgets.QLabel("Color by:", materials_panel)
    mode_combo = QtWidgets.QComboBox(materials_panel)
    mode_combo.addItem("Material", plotter.COLOR_BY_MATERIAL)
    mode_combo.addItem("Cell", plotter.COLOR_BY_CELL)
    mode_layout.addWidget(mode_label)
    mode_layout.addWidget(mode_combo, 1)
    materials_layout.addLayout(mode_layout)

    camera_group = QtWidgets.QGroupBox("Camera", materials_panel)
    camera_layout = QtWidgets.QVBoxLayout(camera_group)

    preset_layout = QtWidgets.QGridLayout()
    iso_btn = QtWidgets.QPushButton("Iso", camera_group)
    x_pos_btn = QtWidgets.QPushButton("+X", camera_group)
    x_neg_btn = QtWidgets.QPushButton("-X", camera_group)
    y_pos_btn = QtWidgets.QPushButton("+Y", camera_group)
    y_neg_btn = QtWidgets.QPushButton("-Y", camera_group)
    z_pos_btn = QtWidgets.QPushButton("+Z", camera_group)
    z_neg_btn = QtWidgets.QPushButton("-Z", camera_group)

    preset_layout.addWidget(iso_btn, 0, 0, 1, 2)
    preset_layout.addWidget(x_pos_btn, 1, 0)
    preset_layout.addWidget(x_neg_btn, 1, 1)
    preset_layout.addWidget(y_pos_btn, 2, 0)
    preset_layout.addWidget(y_neg_btn, 2, 1)
    preset_layout.addWidget(z_pos_btn, 3, 0)
    preset_layout.addWidget(z_neg_btn, 3, 1)
    camera_layout.addLayout(preset_layout)

    rotate_layout, rotate_slider = _make_camera_slider(camera_group, "Rotate", 0.001, 0.02, 0.005)
    pan_layout, pan_slider = _make_camera_slider(camera_group, "Pan", 0.2, 5.0, 1.0)
    zoom_layout, zoom_slider = _make_camera_slider(camera_group, "Zoom", 0.02, 0.5, 0.1)

    camera_layout.addLayout(rotate_layout)
    camera_layout.addLayout(pan_layout)
    camera_layout.addLayout(zoom_layout)

    materials_layout.addWidget(camera_group)

    light_group = QtWidgets.QGroupBox("Lighting", materials_panel)
    light_layout = QtWidgets.QVBoxLayout(light_group)

    light_follow_checkbox = QtWidgets.QCheckBox("Light follows camera", light_group)
    light_follow_checkbox.setChecked(True)
    light_layout.addWidget(light_follow_checkbox)

    light_control_checkbox = QtWidgets.QCheckBox("Light control mode", light_group)
    light_layout.addWidget(light_control_checkbox)

    diffuse_layout = QtWidgets.QHBoxLayout()
    diffuse_label = QtWidgets.QLabel("Diffuse", light_group)
    diffuse_slider = QtWidgets.QSlider(QtCore.Qt.Horizontal, light_group)
    diffuse_slider.setRange(0, 100)
    diffuse_slider.setValue(10)
    diffuse_value = QtWidgets.QLabel("0.10", light_group)
    diffuse_layout.addWidget(diffuse_label)
    diffuse_layout.addWidget(diffuse_slider, 1)
    diffuse_layout.addWidget(diffuse_value)
    light_layout.addLayout(diffuse_layout)

    materials_layout.addWidget(light_group)

    scroll_area = QtWidgets.QScrollArea(materials_panel)
    scroll_area.setWidgetResizable(True)
    scroll_container = QtWidgets.QWidget(scroll_area)
    scroll_layout = QtWidgets.QVBoxLayout(scroll_container)
    scroll_layout.setAlignment(QtCore.Qt.AlignTop)

    if plotter.available:
        _populate_visibility_list(
            scroll_layout,
            plotter.material_list(),
            plotter,
            gl_widget,
        )
        plotter.set_diffuse_fraction(0.1)
    else:
        scroll_layout.addWidget(
            QtWidgets.QLabel("OpenMC not available.", scroll_container)
        )

    mode_combo.currentIndexChanged.connect(
        lambda idx: _on_color_mode_change(idx, mode_combo, plotter, scroll_layout, gl_widget)
    )

    iso_btn.clicked.connect(lambda: gl_widget.set_isometric_view())
    x_pos_btn.clicked.connect(lambda: gl_widget.set_axis_view("x", negative=False))
    x_neg_btn.clicked.connect(lambda: gl_widget.set_axis_view("x", negative=True))
    y_pos_btn.clicked.connect(lambda: gl_widget.set_axis_view("y", negative=False))
    y_neg_btn.clicked.connect(lambda: gl_widget.set_axis_view("y", negative=True))
    z_pos_btn.clicked.connect(lambda: gl_widget.set_axis_view("z", negative=False))
    z_neg_btn.clicked.connect(lambda: gl_widget.set_axis_view("z", negative=True))

    rotate_slider.valueChanged.connect(
        lambda value: _on_camera_speed_change(
            gl_widget,
            rotate_value=value,
            pan_value=pan_slider.value(),
            zoom_value=zoom_slider.value(),
            rotate_min=0.001,
            rotate_max=0.02,
            pan_min=0.2,
            pan_max=5.0,
            zoom_min=0.02,
            zoom_max=0.5,
        )
    )
    pan_slider.valueChanged.connect(
        lambda value: _on_camera_speed_change(
            gl_widget,
            rotate_value=rotate_slider.value(),
            pan_value=value,
            zoom_value=zoom_slider.value(),
            rotate_min=0.001,
            rotate_max=0.02,
            pan_min=0.2,
            pan_max=5.0,
            zoom_min=0.02,
            zoom_max=0.5,
        )
    )
    zoom_slider.valueChanged.connect(
        lambda value: _on_camera_speed_change(
            gl_widget,
            rotate_value=rotate_slider.value(),
            pan_value=pan_slider.value(),
            zoom_value=value,
            rotate_min=0.001,
            rotate_max=0.02,
            pan_min=0.2,
            pan_max=5.0,
            zoom_min=0.02,
            zoom_max=0.5,
        )
    )

    light_follow_checkbox.toggled.connect(
        lambda checked: _on_light_follow_toggle(checked, gl_widget, light_control_checkbox)
    )
    light_control_checkbox.toggled.connect(
        lambda checked: _on_light_control_toggle(checked, gl_widget, light_follow_checkbox)
    )
    diffuse_slider.valueChanged.connect(
        lambda value: _on_diffuse_change(value, diffuse_value, plotter, gl_widget)
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


def _clear_layout(layout):
    while layout.count():
        item = layout.takeAt(0)
        widget = item.widget()
        if widget is not None:
            widget.deleteLater()


def _populate_visibility_list(layout, items, plotter, gl_widget):
    _clear_layout(layout)
    for domain_id, name in items:
        row = QtWidgets.QWidget()
        row_layout = QtWidgets.QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)

        color_btn = QtWidgets.QPushButton()
        color_btn.setFixedSize(20, 20)

        color = plotter.get_color(domain_id)
        _set_color_button_style(color_btn, color)

        label_text = f"{domain_id}"
        if name:
            label_text = f"{domain_id} - {name}"
        label = QtWidgets.QLabel(label_text)
        label.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)

        checkbox = QtWidgets.QCheckBox()
        checkbox.setChecked(True)

        def _make_toggle(did):
            return lambda checked: _on_visibility_toggle(did, checked, plotter, gl_widget)

        def _make_color(did, button):
            return lambda: _on_color_pick(did, button, plotter, gl_widget)

        checkbox.toggled.connect(_make_toggle(domain_id))
        color_btn.clicked.connect(_make_color(domain_id, color_btn))

        row_layout.addWidget(color_btn)
        row_layout.addWidget(label, 1)
        row_layout.addWidget(checkbox)
        layout.addWidget(row)


def _on_visibility_toggle(domain_id, checked, plotter, gl_widget):
    plotter.set_visibility(domain_id, checked)
    gl_widget.request_final_render()


def _on_color_pick(domain_id, button, plotter, gl_widget):
    current = plotter.get_color(domain_id)
    initial = QtGui.QColor(*current)
    color = QtWidgets.QColorDialog.getColor(initial, button, "Select Color")
    if not color.isValid():
        return
    rgb = (color.red(), color.green(), color.blue())
    plotter.set_color(domain_id, rgb)
    _set_color_button_style(button, rgb)
    gl_widget.request_final_render()


def _set_color_button_style(button, rgb):
    r, g, b = rgb
    button.setStyleSheet(
        f"background-color: rgb({r}, {g}, {b}); border: 1px solid #555;"
    )


def _slider_from_scale(value, min_val, max_val):
    if max_val <= min_val:
        return 0
    return int(round((value - min_val) / (max_val - min_val) * 100))


def _scale_from_slider(value, min_val, max_val):
    return min_val + (max_val - min_val) * (value / 100.0)


def _make_camera_slider(parent, label_text, min_val, max_val, default):
    layout = QtWidgets.QHBoxLayout()
    label = QtWidgets.QLabel(label_text, parent)
    slider = QtWidgets.QSlider(QtCore.Qt.Horizontal, parent)
    slider.setRange(0, 100)
    slider.setValue(_slider_from_scale(default, min_val, max_val))
    value_label = QtWidgets.QLabel(parent)

    def _update_value(val):
        actual = _scale_from_slider(val, min_val, max_val)
        value_label.setText(f"{actual:.3f}")

    slider.valueChanged.connect(_update_value)
    _update_value(slider.value())

    layout.addWidget(label)
    layout.addWidget(slider, 1)
    layout.addWidget(value_label)
    return layout, slider


def _on_camera_speed_change(gl_widget, rotate_value, pan_value, zoom_value,
                            rotate_min, rotate_max, pan_min, pan_max, zoom_min, zoom_max):
    rotate = _scale_from_slider(rotate_value, rotate_min, rotate_max)
    pan = _scale_from_slider(pan_value, pan_min, pan_max)
    zoom = _scale_from_slider(zoom_value, zoom_min, zoom_max)
    gl_widget.set_camera_speeds(rotate=rotate, pan=pan, zoom=zoom)


def _on_color_mode_change(index, mode_combo, plotter, layout, gl_widget):
    mode = mode_combo.itemData(index)
    plotter.set_color_by(mode)
    if mode == plotter.COLOR_BY_CELL:
        items = plotter.cell_list()
    else:
        items = plotter.material_list()
    _populate_visibility_list(layout, items, plotter, gl_widget)
    gl_widget.request_final_render()


def _on_light_follow_toggle(checked, gl_widget, light_control_checkbox):
    if checked and light_control_checkbox.isChecked():
        light_control_checkbox.blockSignals(True)
        light_control_checkbox.setChecked(False)
        light_control_checkbox.blockSignals(False)
        gl_widget.set_light_control_mode(False)
    gl_widget.set_light_follows_camera(checked)


def _on_light_control_toggle(checked, gl_widget, light_follow_checkbox):
    if checked and light_follow_checkbox.isChecked():
        light_follow_checkbox.blockSignals(True)
        light_follow_checkbox.setChecked(False)
        light_follow_checkbox.blockSignals(False)
        gl_widget.set_light_follows_camera(False)
    gl_widget.set_light_control_mode(checked)


def _on_diffuse_change(value, label, plotter, gl_widget):
    diffuse = value / 100.0
    label.setText(f"{diffuse:.2f}")
    plotter.set_diffuse_fraction(diffuse)
    gl_widget.request_final_render()


if __name__ == "__main__":
    raise SystemExit(main())
