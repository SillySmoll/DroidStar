/*
	DroidStar Dispatch - Dispatch soundboard tab
	Copyright (C) 2026 DroidStar Dispatch contributors

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Item {
	id: dispatchTab

	property int rev: 0          // bumped when slot settings change, re-evaluates bindings
	property int dstate: 0       // 0 idle, 1 tone playing, 2 live voice
	property string stateText: "Ready"
	readonly property int cols: width > 560 ? 3 : 2
	readonly property var palette12: [
		"#c62828", "#ef6c00", "#f9a825", "#2e7d32", "#558b2f", "#00838f",
		"#1565c0", "#283593", "#6a1b9a", "#ad1457", "#4e342e", "#37474f"
	]

	function textColorFor(c) {
		var col = Qt.color(c);
		var lum = 0.299 * col.r + 0.587 * col.g + 0.114 * col.b;
		return lum > 0.6 ? "black" : "white";
	}

	Connections {
		target: droidstar
		function onDispatch_state_changed(st, txt) {
			dispatchTab.dstate = st;
			dispatchTab.stateText = txt;
		}
		function onDispatch_slots_changed() {
			dispatchTab.rev++;
		}
	}

	Component.onCompleted: {
		toneOnlyRadio.checked = droidstar.dispatch_mode() === 0;
		toneVoiceRadio.checked = droidstar.dispatch_mode() === 1;
		leadSpin.value = droidstar.dispatch_lead();
		tailSpin.value = droidstar.dispatch_tail();
	}

	// ---------- status bar ----------
	Rectangle {
		id: statusBar
		x: 10; y: 6
		width: parent.width - 20
		height: 34
		radius: 6
		color: dispatchTab.dstate === 2 ? "#b71c1c" : (dispatchTab.dstate === 1 ? "#e65100" : "#353535")
		Text {
			anchors.centerIn: parent
			color: "white"
			font.pixelSize: 16
			font.bold: dispatchTab.dstate !== 0
			text: dispatchTab.stateText
		}
	}

	// ---------- mode selector ----------
	Column {
		id: modeBox
		x: 10
		anchors.top: statusBar.bottom
		anchors.topMargin: 4
		width: parent.width - 20
		spacing: 0
		RadioButton {
			id: toneOnlyRadio
			text: qsTr("Tone only, then unkey")
			onToggled: if (checked) droidstar.dispatch_set_mode(0)
		}
		RadioButton {
			id: toneVoiceRadio
			text: qsTr("Tone, then stay keyed for voice")
			onToggled: if (checked) droidstar.dispatch_set_mode(1)
		}
	}

	// ---------- END / STOP button ----------
	Button {
		id: endButton
		x: 10
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 8
		width: parent.width - 20
		height: dispatchTab.dstate === 0 ? 0 : 56
		visible: dispatchTab.dstate !== 0
		background: Rectangle { color: "#800000"; radius: 10 }
		contentItem: Text {
			text: dispatchTab.dstate === 2 ? qsTr("END TX (unkey)") : qsTr("STOP")
			color: "white"
			font.pixelSize: 20
			font.bold: true
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
		onClicked: droidstar.dispatch_end()
	}

	// ---------- tone buttons + timing ----------
	Flickable {
		id: flick
		x: 10
		anchors.top: modeBox.bottom
		anchors.topMargin: 4
		anchors.bottom: endButton.visible ? endButton.top : parent.bottom
		anchors.bottomMargin: 6
		width: parent.width - 20
		contentHeight: contentCol.height
		clip: true
		ScrollBar.vertical: ScrollBar {}

		Column {
			id: contentCol
			width: flick.width
			spacing: 10

			Grid {
				id: grid
				columns: dispatchTab.cols
				spacing: 8
				width: parent.width
				Repeater {
					model: droidstar.dispatch_slot_count()
					delegate: Rectangle {
						id: tile
						required property int index
						readonly property bool assigned: { dispatchTab.rev; return droidstar.dispatch_path(index) !== ""; }
						readonly property string tileColor: { dispatchTab.rev; return droidstar.dispatch_color(index); }
						width: (grid.width - (dispatchTab.cols - 1) * grid.spacing) / dispatchTab.cols
						height: 70
						radius: 10
						color: assigned ? tileColor : "#2e2e2e"
						border.color: tapArea.pressed ? "white" : Qt.darker(color, 1.4)
						border.width: tapArea.pressed ? 3 : 1
						opacity: (dispatchTab.dstate === 1) ? 0.6 : 1.0

						Text {
							anchors.left: parent.left
							anchors.right: gearButton.left
							anchors.leftMargin: 8
							anchors.top: parent.top
							anchors.topMargin: 8
							text: { dispatchTab.rev; return droidstar.dispatch_name(tile.index); }
							color: tile.assigned ? dispatchTab.textColorFor(tile.tileColor) : "#888888"
							font.pixelSize: 15
							font.bold: true
							elide: Text.ElideRight
							maximumLineCount: 2
							wrapMode: Text.Wrap
						}
						Text {
							anchors.left: parent.left
							anchors.leftMargin: 8
							anchors.bottom: parent.bottom
							anchors.bottomMargin: 6
							visible: tile.assigned
							text: { dispatchTab.rev; return "Vol " + droidstar.dispatch_volume(tile.index) + "%"; }
							color: dispatchTab.textColorFor(tile.tileColor)
							opacity: 0.8
							font.pixelSize: 11
						}
						MouseArea {
							id: tapArea
							anchors.fill: parent
							onClicked: {
								if (tile.assigned) droidstar.dispatch_fire(tile.index);
								else editDialog.openFor(tile.index);
							}
						}
						// Gear: edit name / file / color / volume
						Rectangle {
							id: gearButton
							anchors.right: parent.right
							anchors.top: parent.top
							anchors.margins: 4
							width: 30; height: 30; radius: 15
							color: Qt.rgba(0, 0, 0, 0.35)
							Text { anchors.centerIn: parent; text: "⚙"; color: "white"; font.pixelSize: 18 }
							MouseArea {
								anchors.fill: parent
								onClicked: editDialog.openFor(tile.index)
							}
						}
					}
				}
			}

			// Timing
			Row {
				spacing: 8
				Label { text: qsTr("Lead-in ms"); anchors.verticalCenter: parent.verticalCenter }
				SpinBox {
					id: leadSpin
					from: 0; to: 3000; stepSize: 100
					editable: true
					onValueModified: droidstar.dispatch_set_lead(value)
				}
			}
			Row {
				spacing: 8
				Label { text: qsTr("Tail ms   "); anchors.verticalCenter: parent.verticalCenter }
				SpinBox {
					id: tailSpin
					from: 0; to: 3000; stepSize: 100
					editable: true
					onValueModified: droidstar.dispatch_set_tail(value)
				}
			}
			Label {
				width: parent.width
				wrapMode: Text.Wrap
				color: "#aaaaaa"
				font.pixelSize: 11
				text: qsTr("Lead-in = silence sent before the tone so the network has the call up. Tail = time held after the tone before unkeying (tone-only mode). Voice mode unkeys after the TX timeout on the Settings tab. Test on TG 9990 (Parrot) first.")
			}
		}
	}

	// ---------- per-button editor ----------
	FileDialog {
		id: fileDialog
		title: qsTr("Choose a tone / sound file")
		nameFilters: [ "Audio files (*.wav *.mp3 *.m4a *.ogg *.flac *.aac)", "All files (*)" ]
		onAccepted: {
			editDialog.path = selectedFile.toString();
			if (editNameField.text === "" || editNameField.text.startsWith("Empty")) {
				var s = decodeURIComponent(editDialog.path);
				var base = s.substring(s.lastIndexOf("/") + 1);
				var dot = base.lastIndexOf(".");
				editNameField.text = dot > 0 ? base.substring(0, dot) : base;
			}
		}
	}

	ColorDialog {
		id: colorDialog
		title: qsTr("Button color")
		onAccepted: editDialog.color = selectedColor.toString()
	}

	Dialog {
		id: editDialog
		property int slot: 0
		property string path: ""
		property string color: "#1565c0"
		modal: true
		anchors.centerIn: Overlay.overlay
		width: Math.min(dispatchTab.width - 20, 420)
		title: qsTr("Button ") + (slot + 1)
		standardButtons: Dialog.Save | Dialog.Cancel

		function openFor(i) {
			slot = i;
			path = droidstar.dispatch_path(i);
			color = droidstar.dispatch_color(i);
			var n = droidstar.dispatch_name(i);
			editNameField.text = n.startsWith("Empty") ? "" : n;
			volSlider.value = droidstar.dispatch_volume(i);
			open();
		}

		height: Math.min(dispatchTab.height - 10, 620)

		onAccepted: {
			droidstar.dispatch_set_slot(slot, editNameField.text, path, color, Math.round(volSlider.value));
		}

		Flickable {
		id: editFlick
		anchors.fill: parent
		contentHeight: editCol.height
		clip: true
		ScrollBar.vertical: ScrollBar {}
		Column {
			id: editCol
			width: editFlick.width - 12
			spacing: 8

			Label { text: qsTr("Name") }
			TextField {
				id: editNameField
				width: parent.width
				placeholderText: qsTr("e.g. Station 1 Tone-Out")
				selectByMouse: true
			}

			Label { text: qsTr("Sound file") }
			Row {
				width: parent.width
				spacing: 6
				Label {
					width: parent.width - browseButton.width - 6
					anchors.verticalCenter: parent.verticalCenter
					elide: Text.ElideMiddle
					color: editDialog.path === "" ? "#888888" : "white"
					text: {
						if (editDialog.path === "") return qsTr("(none)");
						var s = decodeURIComponent(editDialog.path);
						return s.substring(s.lastIndexOf("/") + 1);
					}
				}
				Button {
					id: browseButton
					text: qsTr("Browse...")
					onClicked: fileDialog.open()
				}
			}

			Label { text: qsTr("Color") }
			Grid {
				columns: 6
				spacing: 6
				Repeater {
					model: dispatchTab.palette12
					delegate: Rectangle {
						required property string modelData
						width: 34; height: 34; radius: 6
						color: modelData
						border.width: Qt.colorEqual(editDialog.color, modelData) ? 3 : 1
						border.color: Qt.colorEqual(editDialog.color, modelData) ? "white" : "#555555"
						MouseArea { anchors.fill: parent; onClicked: editDialog.color = parent.modelData }
					}
				}
			}
			Row {
				spacing: 8
				Rectangle {
					width: 34; height: 34; radius: 6
					color: editDialog.color
					border.color: "white"; border.width: 1
					anchors.verticalCenter: parent.verticalCenter
				}
				Button {
					text: qsTr("Custom color...")
					onClicked: { colorDialog.selectedColor = editDialog.color; colorDialog.open(); }
				}
			}

			Label { text: qsTr("Volume: ") + Math.round(volSlider.value) + "%" }
			Slider {
				id: volSlider
				width: parent.width
				from: 0; to: 200; stepSize: 5
				value: 100
			}
			Label {
				width: parent.width
				wrapMode: Text.Wrap
				color: "#aaaaaa"
				font.pixelSize: 11
				text: qsTr("100% = file as recorded. Above 100% boosts quiet files but can clip loud ones; tones that are too hot sound distorted through AMBE.")
			}

			Button {
				text: qsTr("Clear this button")
				enabled: editDialog.path !== ""
				onClicked: {
					droidstar.dispatch_clear_slot(editDialog.slot);
					editDialog.reject();
				}
			}
		}
		}
	}
}
