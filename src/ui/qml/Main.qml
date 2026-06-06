import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import QtCore

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 1180
    minimumHeight: 760
    visible: true
    title: (playlistModel.name.length > 0 ? playlistModel.name : "Fragments") + (playlistModel.modified ? " *" : "")
    color: theme.appBg

    property string currentThemeName: "Daylight"
    property var themes: ({
        "Daylight": {
            appBg: "#eef2ff",
            block: "#ffffff",
            blockAlt: "#f7f8fe",
            blockAlt2: "#f9faff",
            border: "#dfe4f2",
            borderSoft: "#e5e8f4",
            rowBorder: "#edf0f7",
            selected: "#ebe9ff",
            text: "#121826",
            mutedText: "#687083",
            bodyText: "#394150",
            primary: "#5b4be8",
            primaryStrong: "#2f6fed",
            primaryDark: "#0f3fa6",
            danger: "#c4362e",
            success: "#1f8a4c",
            rail: "#e1e5ee",
            disabled: "#b8bec9",
            videoBg: "#0c0d10"
        },
        "Dark": {
            appBg: "#101217",
            block: "#181b22",
            blockAlt: "#222630",
            blockAlt2: "#1d2129",
            border: "#2d3340",
            borderSoft: "#353c4a",
            rowBorder: "#282e39",
            selected: "#25264a",
            text: "#f3f6fb",
            mutedText: "#9ba5b4",
            bodyText: "#d2d8e3",
            primary: "#7567ff",
            primaryStrong: "#4c8dff",
            primaryDark: "#8fb5ff",
            danger: "#ff7b72",
            success: "#64d47a",
            rail: "#353c49",
            disabled: "#697386",
            videoBg: "#050608"
        }
    })
    property var theme: themes[currentThemeName]

    palette {
        window: theme.appBg
        windowText: theme.text
        base: theme.block
        alternateBase: theme.blockAlt
        text: theme.text
        button: theme.block
        buttonText: theme.text
        highlight: theme.primary
        highlightedText: "#ffffff"
    }

    property int selectedIndex: playlistView.currentIndex
    property var selectedFragment: selectedIndex >= 0 ? playlistModel.get(selectedIndex) : ({})
    property int sourceDurationMs: selectedIndex >= 0 && playback.currentIndex === selectedIndex ? playback.player.duration : 0
    property int draftStartMs: 0
    property int draftEndMs: 0
    property int draftDelayMs: 0
    property string draftDelayColor: "#000000"
    property real draftSpeed: 1.0
    property bool draftAudioEnabled: true
    property real draftVolume: 1.0
    property string draftLabel: ""
    property string draftNotes: ""
    property bool draftDirty: false
    property bool draftValid: draftValidationMessage.length === 0
    property string draftValidationMessage: validateDraft()
    property string pendingAction: ""
    property url pendingRecentPlaylistUrl: ""
    property bool continueAfterSave: false
    property bool forceClose: false
    property var recentPlaylistUrls: appSettings.recentPlaylistUrls || []
    readonly property bool showFragmentLabelColumn: width >= minimumWidth + 160
    readonly property bool showDelayColorCode: width >= minimumWidth + 120
    readonly property int trimTimelineDurationMs: Math.max(sourceDurationMs, draftEndMs, 1)
    readonly property string appVersion: "0.44.6"
    readonly property string appDescription: "A desktop app for building, previewing, and exporting scripted playlists from video and audio fragments."
    readonly property string appCopyright: "(C) 2026, Adjamilton Junior"
    readonly property string appLicense: "GPL-3.0-or-later"
    readonly property string appRepository: "https://github.com/ajunior/fragments"
    property int exportFormatIndex: 0
    property int exportScopeIndex: 0
    property int exportGifFps: 15
    property string exportOutputUrl: ""
    property bool pendingExportSettingsOpen: false
    property string statusBarMessage: ""
    property bool statusBarMessageIsError: false
    FontLoader {
        id: materialSymbols
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf"
    }

    function setStatusBarMessage(message, isError) {
        statusBarMessage = message
        statusBarMessageIsError = !!isError
    }

    function exportIsGif() {
        return exportFormatIndex === 1
    }

    function exportFileSuffix() {
        return exportIsGif() ? "gif" : "mp4"
    }

    function exportSuggestedFileName() {
        const base = playlistModel.nameValid && playlistModel.name.length > 0
            ? playlistModel.name.toLowerCase().replace(/[^a-z0-9_-]+/g, "_")
            : "fragments"
        if (exportScopeIndex === 1 && selectedIndex >= 0) {
            const fragmentBase = draftLabel.length > 0
                ? draftLabel.toLowerCase().replace(/[^a-z0-9_-]+/g, "_")
                : (selectedFragment.fileName || "fragment").toLowerCase().replace(/[^a-z0-9_-]+/g, "_")
            return base + "_" + fragmentBase + "." + exportFileSuffix()
        }
        return base + "." + exportFileSuffix()
    }

    function exportOutputLabel() {
        if (!exportOutputUrl || exportOutputUrl.toString().length === 0)
            return "Choose an output file"

        return exportOutputUrl
    }

    function localPathFromFileUrl(fileUrl) {
        const text = fileUrl ? fileUrl.toString() : ""
        if (!text.startsWith("file://"))
            return text

        let path = text.substring(7)
        if (path.length > 0 && path[0] !== "/")
            path = "/" + path

        try {
            return decodeURIComponent(path)
        } catch (error) {
            return path
        }
    }

    function playlistFileLabel(fileUrl) {
        const text = fileUrl ? fileUrl.toString() : ""
        const slash = text.lastIndexOf("/")
        return slash >= 0 && slash + 1 < text.length ? decodeURIComponent(text.substring(slash + 1)) : text
    }

    function rememberRecentPlaylist(fileUrl) {
        const url = fileUrl ? fileUrl.toString() : ""
        if (url.length === 0)
            return

        const next = [url]
        for (let i = 0; i < recentPlaylistUrls.length && next.length < 8; ++i) {
            const existing = recentPlaylistUrls[i].toString()
            if (existing !== url)
                next.push(existing)
        }

        recentPlaylistUrls = next
        appSettings.recentPlaylistUrls = next
    }

    function forgetRecentPlaylist(fileUrl) {
        const url = fileUrl ? fileUrl.toString() : ""
        const next = []
        for (let i = 0; i < recentPlaylistUrls.length; ++i) {
            const existing = recentPlaylistUrls[i].toString()
            if (existing !== url)
                next.push(existing)
        }

        recentPlaylistUrls = next
        appSettings.recentPlaylistUrls = next
    }

    function openPlaylistFromUrl(fileUrl) {
        if (!fileUrl || fileUrl.toString().length === 0) {
            setStatusBarMessage("No playlist file selected.", true)
            return false
        }

        let loaded = playlistModel.load(fileUrl)
        if (!loaded) {
            const localPath = localPathFromFileUrl(fileUrl)
            if (localPath.length > 0 && localPath !== fileUrl.toString())
                loaded = playlistModel.load(localPath)
        }

        if (!loaded) {
            forgetRecentPlaylist(fileUrl)
            setStatusBarMessage("Could not open " + fileUrl.toString(), true)
            return false
        }

        rememberRecentPlaylist(fileUrl)
        playlistView.currentIndex = playlistModel.count > 0 ? 0 : -1
        loadDraftFromSelection()
        setStatusBarMessage("Opened " + playlistModel.name + " (" + playlistModel.count + " fragments).", false)
        return true
    }

    function createPlaylistFromMenu() {
        closeProjectMenu()
        if (playlistModel.modified) {
            pendingAction = "new"
            unsavedDialog.show()
            return
        }

        playback.stop()
        playlistModel.newPlaylist()
        playlistView.currentIndex = -1
        loadDraftFromSelection()
        setStatusBarMessage("Created new playlist.", false)
    }

    function continueAfterUnsavedConfirmation() {
        const action = pendingAction
        pendingAction = ""

        if (action === "open") {
            openDialog.open()
        } else if (action === "openRecent") {
            openPlaylistFromUrl(pendingRecentPlaylistUrl)
            pendingRecentPlaylistUrl = ""
        } else if (action === "new") {
            playback.stop()
            playlistModel.newPlaylist()
            playlistView.currentIndex = -1
            loadDraftFromSelection()
            setStatusBarMessage("Created new playlist.", false)
        } else if (action === "close") {
            forceClose = true
            root.close()
        }
    }

    function savePlaylistAndContinueAfterConfirmation() {
        if (playlistModel.fileUrl && playlistModel.fileUrl.toString().length > 0) {
            if (savePlaylistFile(playlistModel.fileUrl))
                continueAfterUnsavedConfirmation()
        } else {
            continueAfterSave = true
            openSaveDialog()
        }
    }

    function openExportFileDialog() {
        let folder = exportFileDialog.currentFolder
        if (!folder || folder.toString().length === 0) {
            folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
            exportFileDialog.currentFolder = folder
        }

        let folderUrl = folder.toString()
        if (!folderUrl.endsWith("/"))
            folderUrl += "/"

        const suggestedFileUrl = folderUrl + exportSuggestedFileName()
        exportFileDialog.currentFile = suggestedFileUrl
        exportFileDialog.selectedFile = suggestedFileUrl
        exportFileDialog.open()
    }

    function startExportFromDialog() {
        if (!exportOutputUrl || exportOutputUrl.length === 0) {
            setStatusBarMessage("Choose an output file.", true)
            return
        }

        const exportRow = exportScopeIndex === 1 ? selectedIndex : -1
        if (exportScopeIndex === 1 && exportRow < 0) {
            setStatusBarMessage("Select a fragment to export.", true)
            return
        }

        const ok = exportIsGif()
            ? exporter.exportGifTo(exportOutputUrl, exportGifFps, exportRow)
            : exporter.exportTo(exportOutputUrl, exportGifFps, exportRow)

        if (ok)
            exportSettingsDialogItem().close()
    }

    function exportSettingsDialogItem() {
        return exportSettingsDialogLoader.active ? exportSettingsDialogLoader.item : null
    }

    function openExportSettingsDialog(scope) {
        exportScopeIndex = scope !== undefined ? scope : (selectedIndex >= 0 ? 1 : 0)
        pendingExportSettingsOpen = true
        if (!exportSettingsDialogLoader.active)
            exportSettingsDialogLoader.active = true
        else
            Qt.callLater(showExportSettingsDialog)
    }

    function showExportSettingsDialog() {
        const dialog = exportSettingsDialogItem()
        if (!dialog)
            return

        pendingExportSettingsOpen = false
        dialog.show()
        dialog.raise()
        dialog.requestActivate()
    }

    function openFragmentExportDialog(index) {
        if (index < 0) {
            setStatusBarMessage("Select a fragment to export.", true)
            return
        }
        selectFragment(index)
        openExportSettingsDialog(1)
    }

    function openProjectMenu() {
        if (projectMenuPopup.visible) {
            projectMenuPopup.close()
            return
        }

        projectMenuPopup.open()
    }

    function closeProjectMenu() {
        projectMenuPopup.close()
    }

    function triggerOpenPlaylistFromMenu() {
        closeProjectMenu()
        if (playlistModel.modified) {
            pendingAction = "open"
            unsavedDialog.show()
            return
        }

        openDialog.open()
    }

    function recentPlaylistUrlAt(index) {
        return index >= 0 && index < recentPlaylistUrls.length ? recentPlaylistUrls[index] : ""
    }

    function currentStatusBar() {
        if (statusBarMessage.length > 0)
            return { text: statusBarMessage, error: statusBarMessageIsError }

        if (selectedIndex >= 0) {
            if (!draftValid)
                return { text: draftValidationMessage, error: true }

            if (selectedFragment.sourceStatus === "Missing")
                return { text: "Source missing: " + (selectedFragment.fileName || "Unknown source"), error: true }

            if (selectedFragment.sourceStatus && selectedFragment.sourceStatus !== "Available")
                return { text: selectedFragment.sourceStatus, error: false }
        }

        if (!exporter.ffmpegAvailable) {
            return {
                text: exporter.exportReadinessMessage.length > 0
                    ? exporter.exportReadinessMessage
                    : "FFmpeg is missing, so export and GIF are not available.",
                error: true
            }
        }

        return { text: "", error: false }
    }

    Settings {
        id: appSettings
        category: "Application"
        property var recentPlaylistUrls: []
    }

    onSelectedIndexChanged: loadDraftFromSelection()
    Component.onCompleted: loadDraftFromSelection()
    onClosing: function(close) {
        if (forceClose || !playlistModel.modified) {
            return
        }

        close.accepted = false
        pendingAction = "close"
        unsavedDialog.show()
    }

    FileDialog {
        id: mediaDialog
        title: "Add Media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.mp3 *.wav *.flac)", "All files (*)"]
        onAccepted: {
            for (let i = 0; i < selectedFiles.length; ++i)
                playlistModel.addFragment(selectedFiles[i])
            if (playlistView.currentIndex < 0 && playlistModel.count > 0)
                playlistView.currentIndex = 0
        }
    }

    FileDialog {
        id: relinkDialog
        title: "Relink Source"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.mp3 *.wav *.flac)", "All files (*)"]
        onAccepted: {
            if (selectedIndex >= 0) {
                playlistModel.relinkSource(selectedIndex, selectedFile)
                loadDraftFromSelection()
                playback.setVideoSink(videoOutput.videoSink)
                playback.cue(selectedIndex)
            }
        }
    }

    FileDialog {
        id: openDialog
        title: "Open Playlist"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Fragments playlist (*.json)", "All files (*)"]
        onAccepted: {
            const fileUrl = selectedFile
            setStatusBarMessage("Opening " + fileUrl.toString(), false)
            try {
                root.openPlaylistFromUrl(fileUrl)
            } catch (error) {
                setStatusBarMessage(String(error), true)
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save Playlist"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Fragments playlist (*.json)", "All files (*)"]
        defaultSuffix: "json"
        onAccepted: {
            savePlaylistFile(selectedFile)
            if (continueAfterSave) {
                continueAfterSave = false
                continueAfterUnsavedConfirmation()
            }
        }
    }

    FileDialog {
        id: exportFileDialog
        title: "Choose Export File"
        fileMode: FileDialog.SaveFile
        nameFilters: exportIsGif() ? ["GIF image (*.gif)", "All files (*)"] : ["MP4 video (*.mp4)", "All files (*)"]
        defaultSuffix: exportFileSuffix()
        onAccepted: exportOutputUrl = selectedFile.toString()
    }

    Loader {
        id: exportSettingsDialogLoader
        active: false
        sourceComponent: exportSettingsDialogComponent
        onLoaded: if (pendingExportSettingsOpen) Qt.callLater(showExportSettingsDialog)
    }

    Component {
        id: exportSettingsDialogComponent

        Window {
            title: exportScopeIndex === 1 ? "Export Fragment" : "Export Playlist"
            width: 540
            height: 320
            minimumWidth: 540
            minimumHeight: 320
            maximumWidth: 540
            maximumHeight: 320
            x: Math.round((root.width - width) / 2)
            y: Math.round((root.height - height) / 2)
            transientParent: root
            color: theme.appBg

            onVisibleChanged: {
                if (visible) {
                    x = root.x + Math.round((root.width - width) / 2)
                    y = root.y + Math.round((root.height - height) / 2)
                }
            }

            Shortcut {
                sequence: "Esc"
                onActivated: close()
            }

            Rectangle {
                anchors.fill: parent
                color: theme.appBg
                radius: 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: theme.block
                        border.color: theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: "Scope"
                                color: theme.bodyText
                                Layout.preferredWidth: 92
                            }

                            AppButton {
                                text: "Playlist"
                                primary: exportScopeIndex === 0
                                Layout.fillWidth: true
                                onClicked: exportScopeIndex = 0
                            }

                            AppButton {
                                text: "Fragment"
                                primary: exportScopeIndex === 1
                                enabled: selectedIndex >= 0
                                Layout.fillWidth: true
                                onClicked: exportScopeIndex = 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: "Format"
                                color: theme.bodyText
                                Layout.preferredWidth: 92
                            }

                            AppButton {
                                text: "MP4"
                                primary: !exportIsGif()
                                Layout.fillWidth: true
                                onClicked: exportFormatIndex = 0
                            }

                            AppButton {
                                text: "GIF"
                                primary: exportIsGif()
                                Layout.fillWidth: true
                                onClicked: exportFormatIndex = 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: "Output"
                                color: theme.bodyText
                                Layout.preferredWidth: 92
                            }

                            TextField {
                                text: exportOutputLabel()
                                readOnly: true
                                color: theme.text
                                selectedTextColor: "#ffffff"
                                selectionColor: theme.primary
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                implicitHeight: 34
                                background: Rectangle {
                                    radius: 10
                                    color: theme.block
                                    border.color: theme.border
                                }
                            }

                            AppButton {
                                text: "Browse"
                                onClicked: openExportFileDialog()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            visible: exportIsGif()

                            Label {
                                text: "GIF FPS"
                                color: theme.bodyText
                                Layout.preferredWidth: 92
                            }

                            SpinBox {
                                from: 1
                                to: 60
                                value: exportGifFps
                                editable: true
                                Layout.preferredWidth: 120
                                onValueChanged: exportGifFps = value
                            }

                            Label {
                                text: "Higher FPS makes the GIF smoother but heavier."
                                color: theme.mutedText
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            text: exportIsGif()
                                ? "GIF export is silent and uses the selected frame rate."
                                : "MP4 export keeps audio when the fragments have it."
                            color: theme.mutedText
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        Item { Layout.fillWidth: true }
                        AppButton {
                            text: "Cancel"
                            onClicked: close()
                        }
                        AppButton {
                            text: "Export"
                            primary: true
                            enabled: !exporter.running
                                     && playlistModel.valid
                                     && (exportScopeIndex === 0 ? playlistModel.count > 0 : selectedIndex >= 0)
                                     && (!exportIsGif() ? exporter.mp4ExportAvailable : exporter.gifExportAvailable)
                            toolTip: exportIsGif() ? exporter.gifExportReadinessMessage : exporter.mp4ExportReadinessMessage
                            onClicked: startExportFromDialog()
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: recoveryFolderDialog
        title: "Find Missing Media"
        onAccepted: {
            const count = playlistModel.relinkMissingFromFolder(selectedFolder)
            setStatusBarMessage(count > 0 ? "Relinked " + count + " missing source" + (count === 1 ? "" : "s") : "No matching files found", count <= 0)
            missingMediaDialog.close()
            loadDraftFromSelection()
        }
    }

    ColorDialog {
        id: delayColorDialog
        title: "Delay Background Color"
        selectedColor: draftDelayColor
        onAccepted: {
            draftDelayColor = colorToHex(selectedColor)
            draftDirty = true
        }
    }

    Dialog {
        id: exportStatusDialog
        title: "Export"
        modal: true
        width: 460
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        closePolicy: exporter.running ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: theme.block
            radius: 14
            border.color: theme.border
        }

        header: Label {
            text: exportStatusDialog.title
            color: theme.text
            font.pixelSize: 18
            font.bold: true
            padding: 18
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: exporter.status
                color: theme.bodyText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ProgressBar {
                from: 0
                to: 1
                value: exporter.progress
                Layout.fillWidth: true
            }
        }

        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton {
                text: exporter.running ? "Cancel" : "Close"
                onClicked: exporter.running ? exporter.cancel() : exportStatusDialog.close()
            }
        }
    }

    Dialog {
        id: missingMediaDialog
        title: "Missing Media"
        modal: true
        width: 620
        height: 420
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        background: Rectangle {
            color: theme.block
            radius: 14
            border.color: theme.border
        }

        header: Label {
            text: missingMediaDialog.title
            color: theme.text
            font.pixelSize: 18
            font.bold: true
            padding: 18
        }

        contentItem: ListView {
            clip: true
            spacing: 6
            model: playlistModel.missingSources()

            delegate: Rectangle {
                required property int row
                required property string fileName
                required property string path
                required property string label

                width: ListView.view.width
                height: 48
                radius: 8
                color: theme.blockAlt
                border.color: theme.borderSoft

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: label.length > 0 ? label : fileName
                        color: theme.text
                        elide: Text.ElideRight
                        Layout.preferredWidth: 180
                    }

                    Label {
                        text: path
                        color: theme.mutedText
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    AppButton {
                        text: "Relink"
                        onClicked: {
                            playlistView.currentIndex = row
                            missingMediaDialog.close()
                            relinkDialog.open()
                        }
                    }
                }
            }
        }

        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton { text: "Close"; onClicked: missingMediaDialog.close() }
            AppButton {
                text: "Find Folder"
                primary: true
                onClicked: recoveryFolderDialog.open()
            }
        }
    }

    Window {
        id: unsavedDialog
        title: "Unsaved Changes"
        width: 460
        height: 190
        minimumWidth: 460
        minimumHeight: 190
        maximumWidth: 460
        maximumHeight: 190
        visible: false
        modality: Qt.ApplicationModal
        transientParent: root
        color: theme.appBg

        onVisibleChanged: {
            if (visible) {
                x = root.x + Math.round((root.width - width) / 2)
                y = root.y + Math.round((root.height - height) / 2)
                raise()
                requestActivate()
            }
        }

        Shortcut {
            sequence: "Esc"
            onActivated: {
                pendingAction = ""
                unsavedDialog.close()
            }
        }

        Rectangle {
            anchors.fill: parent
            color: theme.appBg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: theme.block
                    border.color: theme.border

                    Label {
                        anchors.fill: parent
                        anchors.margins: 14
                        text: "You have unsaved changes. Save them before continuing?"
                        color: theme.bodyText
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    AppButton {
                        text: "Cancel"
                        onClicked: {
                            pendingAction = ""
                            unsavedDialog.close()
                        }
                    }

                    AppButton {
                        text: "Discard"
                        onClicked: {
                            unsavedDialog.close()
                            continueAfterUnsavedConfirmation()
                        }
                    }

                    AppButton {
                        text: "Save"
                        primary: true
                        enabled: playlistModel.hasPlaylist && playlistModel.nameValid
                        onClicked: {
                            unsavedDialog.close()
                            savePlaylistAndContinueAfterConfirmation()
                        }
                    }
                }
            }
        }
    }

    Window {
        id: aboutWindow
        title: "About Fragments"
        width: 900
        height: 680
        minimumWidth: 900
        minimumHeight: 680
        maximumWidth: 900
        maximumHeight: 680
        visible: false
        modality: Qt.ApplicationModal
        transientParent: root
        color: theme.appBg

        onVisibleChanged: {
            if (visible) {
                x = root.x + Math.round((root.width - width) / 2)
                y = root.y + Math.round((root.height - height) / 2)
                if (aboutScroll.contentItem) {
                    aboutScroll.contentItem.contentX = 0
                    aboutScroll.contentItem.contentY = 0
                }
            }
        }

        Shortcut {
            sequence: "Esc"
            onActivated: aboutWindow.close()
        }

        Rectangle {
            anchors.fill: parent
            color: theme.appBg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 168
                    color: currentThemeName === "Dark" ? "#0b1020" : "#101827"
                    radius: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 18

                        Rectangle {
                            Layout.preferredWidth: 76
                            Layout.preferredHeight: 76
                            radius: 8
                            color: "#ffffff"

                            Image {
                                anchors.centerIn: parent
                                width: 54
                                height: 54
                                source: "qrc:/assets/icons/io.github.ajunior.fragments.svg"
                                fillMode: Image.PreserveAspectFit
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "Fragments"
                                color: "#ffffff"
                                font.pixelSize: 34
                                font.bold: true
                            }

                            Label {
                                text: appDescription
                                color: "#c7d2e5"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                spacing: 8

                                AboutPill { text: "Version " + appVersion }
                                AboutPill { text: appLicense }
                            }
                        }
                    }
                }

                ScrollView {
                    id: aboutScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        id: aboutScrollContent
                        width: aboutScroll.availableWidth - 36
                        x: 18
                        y: 8
                        spacing: 14

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 8
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 312
                            spacing: 14

                            AboutCard {
                                title: "What It Does"
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Label {
                                        text: "Fragments helps you build repeatable playlists from small pieces of video and audio."
                                        color: theme.bodyText
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: "Trim exact ranges, reorder fragments, add delay, mute or keep audio, preview changes, recover missing media, and export the final playlist."
                                        color: theme.bodyText
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: "Use it when you need a fast fragment-based workflow instead of a full timeline editor. The original media files are not modified."
                                        color: theme.bodyText
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: appCopyright
                                        color: theme.bodyText
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1
                                Layout.topMargin: 8
                                Layout.bottomMargin: 8
                                color: theme.borderSoft
                                opacity: 0.95
                            }

                            AboutCard {
                                title: "Basic Instructions"
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    InstructionRow { number: "1"; text: "Add local video or audio files to create fragments." }
                                    InstructionRow { number: "2"; text: "Select a fragment, scrub the preview, and set start/end." }
                                    InstructionRow { number: "3"; text: "Tune delay, delay color, audio, volume, speed, label, and notes." }
                                    InstructionRow { number: "4"; text: "Preview the draft, then save the fragment changes." }
                                    InstructionRow { number: "5"; text: "Save the playlist or export it as MP4/GIF with FFmpeg installed." }
                                }
                            }
                        }

                        AboutCard {
                            title: "Shortcuts"
                            Layout.fillWidth: true

                            GridLayout {
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 12
                                Layout.fillWidth: true

                                ShortcutInfo { keys: "Ctrl+O"; action: "Open playlist" }
                                ShortcutInfo { keys: "Ctrl+S"; action: "Save playlist" }
                                ShortcutInfo { keys: "Ctrl+Shift+S"; action: "Save as" }
                                ShortcutInfo { keys: "Ctrl+Z"; action: "Undo" }
                                ShortcutInfo { keys: "Ctrl+Y"; action: "Redo" }
                                ShortcutInfo { keys: "P"; action: "Preview selected fragment" }
                                ShortcutInfo { keys: "Space"; action: "Pause or resume playback" }
                                ShortcutInfo { keys: "Delete"; action: "Remove selected fragment" }
                                ShortcutInfo { keys: "Ctrl+D"; action: "Duplicate selected fragment" }
                                ShortcutInfo { keys: "Ctrl+Up / Ctrl+Down"; action: "Move selected fragment" }
                                ShortcutInfo { keys: "I"; action: "Set draft start from playhead" }
                                ShortcutInfo { keys: "O"; action: "Set draft end from playhead" }
                                ShortcutInfo { keys: "R"; action: "Reset unsaved draft changes" }
                                ShortcutInfo { keys: "Ctrl+Enter"; action: "Save selected fragment draft" }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppButton {
                                text: "View Source"
                                onClicked: Qt.openUrlExternally(appRepository)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Made with <font color=\"#e25555\">&hearts;</font> and released as open source software."
                                color: theme.bodyText
                                textFormat: Text.RichText
                                horizontalAlignment: Text.AlignHCenter
                            }

                            AppButton {
                                text: "Close"
                                primary: true
                                onClicked: aboutWindow.close()
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 18
                        }
                    }
                }
            }
        }
    }

    Window {
        id: playbackWindow
        width: 960
        height: 540
        minimumWidth: 640
        minimumHeight: 360
        title: "Fragments Playback"
        color: "#000000"
        property bool controlsVisible: true

        onVisibleChanged: {
            if (visible) {
                playback.setVideoSink(playbackWindowVideo.videoSink)
                showPlaybackControls()
            } else {
                if (visibility === Window.FullScreen)
                    showNormal()
                playback.setVideoSink(videoOutput.videoSink)
            }
        }

        onVisibilityChanged: showPlaybackControls()

        Shortcut {
            sequence: "Space"
            onActivated: playback.playing ? playback.pause() : playback.resume()
        }

        Shortcut {
            sequence: "Esc"
            onActivated: {
                if (playbackWindow.visibility === Window.FullScreen)
                    playbackWindow.showNormal()
                else
                    playbackWindow.close()
            }
        }

        Shortcut {
            sequence: "F11"
            onActivated: togglePlaybackFullscreen()
        }

        Rectangle {
            anchors.fill: parent
            color: "#000000"

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                onPositionChanged: showPlaybackControls()
            }

            VideoOutput {
                id: playbackWindowVideo
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
            }

            DelayPreviewOverlay {
                anchors.fill: parent
                active: playback.delayActive
                delayColor: playback.currentDelayColor
                remainingMs: playback.delayRemainingMs
                progress: playback.delayProgress
                compact: false
            }

            Rectangle {
                id: playbackControls
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 58
                color: "#99000000"
                opacity: playbackWindow.controlsVisible || playbackWindow.visibility !== Window.FullScreen || !playback.playing ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: 180 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    Label {
                        text: playback.currentIndex >= 0 ? "Fragment " + (playback.currentIndex + 1) : "Ready"
                        color: "#ffffff"
                        font.pixelSize: 14
                        Layout.minimumWidth: 110
                    }

                    Label {
                        text: formatTime(playback.player.position / 1000)
                        color: "#ffffff"
                        font.family: "monospace"
                        Layout.preferredWidth: 78
                    }

                    AppSlider {
                        Layout.preferredWidth: 260
                        from: playback.currentStart * 1000
                        to: Math.max(from + 1, playback.currentEnd * 1000)
                        value: Math.max(from, Math.min(to, playback.player.position))
                        onMoved: playback.player.position = value
                    }

                    Label {
                        text: formatTime(playback.currentEnd)
                        color: "#ffffff"
                        font.family: "monospace"
                        Layout.preferredWidth: 78
                    }

                    AppButton {
                        text: "Pause"
                        enabled: playback.playing
                        onClicked: playback.pause()
                    }

                    AppButton {
                        text: "Stop"
                        onClicked: playback.stop()
                    }

                    AppButton {
                        text: playbackWindow.visibility === Window.FullScreen ? "Window" : "Fullscreen"
                        onClicked: togglePlaybackFullscreen()
                    }
                }
            }
        }

        Timer {
            id: playbackControlsHideTimer
            interval: 2000
            repeat: false
            onTriggered: {
                if (playbackWindow.visibility === Window.FullScreen && playback.playing)
                    playbackWindow.controlsVisible = false
            }
        }
    }

    Connections {
        target: playlistModel
        function onLoadFailed(message) { setStatusBarMessage("Load failed: " + message, true) }
        function onSaveFailed(message) { setStatusBarMessage("Save failed: " + message, true) }
    }

    Connections {
        target: playback
        function onPlaybackError(message) { setStatusBarMessage("Playback failed: " + message, true) }
        function onPlayingChanged() { showPlaybackControls() }
    }

    Connections {
        target: exporter
        function onRunningChanged() {
            if (exporter.running)
                exportStatusDialog.open()
        }
        function onExportFinished(outputUrl) {
            setStatusBarMessage("Exported " + recentPlaylistLabel(outputUrl), false)
        }
        function onExportFailed(message) {
            setStatusBarMessage("Export failed: " + message, true)
            exportStatusDialog.open()
        }
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: requestOpenPlaylist() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: savePlaylist() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: openSaveDialog() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: playlistModel.canUndo; onActivated: undoPlaylist() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: playlistModel.canRedo; onActivated: redoPlaylist() }
    Shortcut { sequence: "Ctrl+Y"; enabled: playlistModel.canRedo; onActivated: redoPlaylist() }
    Shortcut { sequence: "Space"; onActivated: playback.playing ? playback.pause() : playback.resume() }
    Shortcut { sequence: "Delete"; onActivated: removeSelectedFragment() }
    Shortcut { sequence: "Ctrl+D"; onActivated: duplicateSelectedFragment() }
    Shortcut { sequence: "Ctrl+Up"; onActivated: moveSelectedFragment(-1) }
    Shortcut { sequence: "Ctrl+Down"; onActivated: moveSelectedFragment(1) }
    Shortcut { sequence: "PgUp"; onActivated: moveSelectedFragment(-1) }
    Shortcut { sequence: "PgDown"; onActivated: moveSelectedFragment(1) }
    Shortcut { sequence: "P"; onActivated: previewSelectedDraft() }
    Shortcut { sequence: "I"; onActivated: setDraftStartFromCurrent() }
    Shortcut { sequence: "O"; onActivated: setDraftEndFromCurrent() }
    Shortcut { sequence: "R"; onActivated: if (selectedIndex >= 0 && draftDirty) loadDraftFromSelection() }
    Shortcut { sequence: "Ctrl+Return"; onActivated: if (selectedIndex >= 0 && draftDirty && draftValid) updateSelectedMetadata() }
    Shortcut { sequence: "Ctrl+Enter"; onActivated: if (selectedIndex >= 0 && draftDirty && draftValid) updateSelectedMetadata() }
    Shortcut { sequence: "Esc"; enabled: projectMenuPopup.visible; onActivated: closeProjectMenu() }

    header: Pane {
        height: 76
        padding: 0
        background: Rectangle { color: theme.appBg }

        Rectangle {
            id: topBar
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            color: theme.block
            radius: 12
            border.color: theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                IconButton {
                    id: projectMenuButton
                    iconName: "menu"
                    toolTip: "Project"
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: openProjectMenu()
                }
                Item { Layout.fillWidth: true }
                Item {
                    visible: playlistModel.hasPlaylist
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    RowLayout {
                        spacing: 8
                        anchors.centerIn: parent

                        Label {
                            text: "Playlist"
                            color: theme.bodyText
                            font.pixelSize: 13
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        TextField {
                            id: playlistNameField
                            text: playlistModel.name
                            placeholderText: "Untitled"
                            color: theme.text
                            selectedTextColor: "#ffffff"
                            selectionColor: theme.primary
                            font.pixelSize: 13
                            verticalAlignment: TextInput.AlignVCenter
                            implicitHeight: 34
                            Layout.preferredWidth: 190
                            Layout.alignment: Qt.AlignVCenter
                            validator: RegularExpressionValidator {
                                regularExpression: /[A-Za-z_][A-Za-z0-9_-]*/
                            }
                            onEditingFinished: {
                                if (acceptableInput)
                                    playlistModel.name = text
                            }
                            ToolTip.visible: activeFocus && !acceptableInput
                            ToolTip.text: playlistModel.nameValidationMessage

                            Connections {
                                target: playlistModel
                                function onNameChanged() {
                                    if (!playlistNameField.activeFocus)
                                        playlistNameField.text = playlistModel.name
                                }
                            }

                            background: Rectangle {
                                radius: 10
                                color: playlistNameField.activeFocus ? theme.blockAlt : theme.block
                                border.color: playlistNameField.activeFocus && !playlistNameField.acceptableInput ? theme.danger : (playlistNameField.activeFocus ? theme.primary : theme.border)
                            }
                        }
                        Label {
                            text: "Repeat"
                            color: theme.bodyText
                            font.pixelSize: 13
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        AppSwitch {
                            checked: playlistModel.repeat
                            Layout.alignment: Qt.AlignVCenter
                            onToggled: playlistModel.repeat = checked
                        }
                        Item { Layout.preferredWidth: 18 }
                        Label {
                            text: "Created:"
                            color: theme.bodyText
                            font.pixelSize: 13
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: formatDateTime(playlistModel.createdAt)
                            color: theme.bodyText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Item { Layout.preferredWidth: 5 }
                        Label {
                            text: "Modified:"
                            color: theme.bodyText
                            font.pixelSize: 13
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: formatDateTime(playlistModel.updatedAt)
                            color: theme.bodyText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    IconButton {
                        iconName: currentThemeName === "Dark" ? "moon" : "sun"
                        toolTip: currentThemeName === "Dark" ? "Dark theme" : "Daylight theme"
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: currentThemeName = currentThemeName === "Dark" ? "Daylight" : "Dark"
                    }
                    IconButton {
                        iconName: "info"
                        toolTip: "About"
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            aboutWindow.show()
                            aboutWindow.raise()
                            aboutWindow.requestActivate()
                        }
                    }
                }

            }
        }
    }

    Popup {
        id: projectMenuPopup
        parent: topBar
        x: Math.max(12, Math.min(parent.width - width - 12,
                                  projectMenuButton.mapToItem(parent, 0, 0).x))
        y: projectMenuButton.mapToItem(parent, 0, projectMenuButton.height + 14).y
        width: 260
        height: projectMenuContent.implicitHeight + topPadding + bottomPadding
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 10
            color: theme.block
            border.color: theme.border
        }

        contentItem: Column {
            id: projectMenuContent
            spacing: 2

            ProjectMenuItem { text: "New"; onTriggered: createPlaylistFromMenu() }
            ProjectMenuItem { text: "Open"; onTriggered: triggerOpenPlaylistFromMenu() }

            ProjectMenuSeparator {}
            ProjectMenuItem { text: "Undo"; enabled: playlistModel.canUndo; onTriggered: { closeProjectMenu(); undoPlaylist() } }
            ProjectMenuItem { text: "Redo"; enabled: playlistModel.canRedo; onTriggered: { closeProjectMenu(); redoPlaylist() } }

            ProjectMenuSeparator {}
            ProjectMenuItem { text: recentPlaylistUrls.length > 0 ? "Recent" : "Recent (none)"; enabled: false }

            Repeater {
                model: recentPlaylistUrls
                ProjectMenuItem {
                    required property string modelData
                    text: playlistFileLabel(modelData)
                    onTriggered: {
                        closeProjectMenu()
                        openPlaylistFromUrl(modelData)
                    }
                }
            }

            ProjectMenuItem {
                text: "Clear Recent"
                enabled: recentPlaylistUrls.length > 0
                onTriggered: {
                    closeProjectMenu()
                    clearRecentPlaylists()
                }
            }

            ProjectMenuSeparator {}
            ProjectMenuItem {
                text: "Save"
                enabled: playlistModel.hasPlaylist && playlistModel.nameValid
                onTriggered: {
                    closeProjectMenu()
                    savePlaylist()
                }
            }
            ProjectMenuItem {
                text: "Save As"
                enabled: playlistModel.hasPlaylist && playlistModel.nameValid
                onTriggered: {
                    closeProjectMenu()
                    openSaveDialog()
                }
            }

            ProjectMenuSeparator {}
            ProjectMenuItem {
                text: exporter.running ? "Exporting" : "Export"
                enabled: playlistModel.count > 0 && playlistModel.valid && !exporter.running
                toolTip: exporter.ffmpegAvailable ? "Open export settings" : exporter.exportReadinessMessage
                onTriggered: {
                    closeProjectMenu()
                    openExportSettingsDialog()
                }
            }
            ProjectMenuItem {
                text: "Recover Missing Media"
                enabled: playlistModel.missingSources().length > 0
                onTriggered: {
                    closeProjectMenu()
                    missingMediaDialog.open()
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 4
        anchors.bottomMargin: 12
        Pane {
            id: leftPane
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: statusBar.top
            anchors.bottomMargin: 8
            width: Math.round((parent.width - 14) * 0.73)
            padding: 12
            background: Rectangle {
                color: theme.block
                radius: 14
                border.color: theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Fragments"
                        color: theme.text
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        text: playlistModel.count + " items"
                        color: theme.mutedText
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }

                    AppButton {
                        text: "Play All"
                        primary: true
                        enabled: playlistModel.count > 0 && playlistModel.valid
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            playbackWindow.show()
                            playbackWindow.raise()
                            playbackWindow.requestActivate()
                            playback.setVideoSink(playbackWindowVideo.videoSink)
                            playback.play(0)
                        }
                    }

                    AppButton {
                        text: "Previous"
                        enabled: playlistModel.count > 0
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: playback.previous()
                    }

                    AppButton {
                        text: "Next"
                        enabled: playlistModel.count > 0
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: playback.next()
                    }

                    AppButton {
                        text: "+"
                        primary: true
                        implicitWidth: 34
                        enabled: playlistModel.hasPlaylist
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: mediaDialog.open()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: theme.blockAlt
                    radius: 10
                    border.color: theme.borderSoft

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 0

                        Label { text: "#"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 44 }
                        Label {
                            text: "Label"
                            visible: root.showFragmentLabelColumn
                            color: theme.mutedText
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.preferredWidth: root.showFragmentLabelColumn ? 160 : 0
                        }
                        Label { text: "File"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight; Layout.preferredWidth: 240; Layout.fillWidth: true }
                        Label { text: "Start"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 104 }
                        Label { text: "End"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 104 }
                        Label { text: "Duration"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 108 }
                        Label { text: "Delay"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 78 }
                        Label { text: "Audio"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 70 }
                        Label { text: "Volume"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 78 }
                        Label { text: "Speed"; color: theme.mutedText; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 72 }
                    }
                }

                ListView {
                    id: playlistView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 0
                    model: playlistModel
                    onCurrentIndexChanged: {
                        loadDraftFromSelection()
                        cueSelectionForPreview()
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 48, 360)
                        height: 72
                        visible: playlistModel.count === 0
                        color: "transparent"

                        Label {
                            anchors.centerIn: parent
                            text: "Add media to create the first fragment"
                            color: theme.mutedText
                            font.pixelSize: 16
                        }
                    }

                    delegate: Item {
                        id: fragmentRow
                        required property int index
                        required property string fileName
                        required property double start
                        required property double end
                        required property double duration
                        required property double delayBefore
                        required property bool audioEnabled
                        required property double volume
                        required property double speed
                        required property string label
                        required property string notes
                        required property bool valid
                        required property string validationMessage

                        width: ListView.view.width
                        height: 38

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: fragmentRow.ListView.isCurrentItem ? theme.selected
                                   : (rowHover.hovered ? theme.blockAlt
                                   : (!valid ? theme.blockAlt : (index % 2 === 0 ? theme.block : theme.blockAlt2)))
                            border.color: fragmentRow.ListView.isCurrentItem || playback.currentIndex === index ? theme.primary : (!valid ? theme.danger : theme.rowBorder)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 0

                            Label { text: valid ? index + 1 : "!"; color: valid ? theme.mutedText : theme.danger; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 44 }
                            Label {
                                text: label.length > 0 ? label : "-"
                                visible: root.showFragmentLabelColumn
                                color: label.length > 0 ? theme.text : theme.mutedText
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.preferredWidth: root.showFragmentLabelColumn ? 160 : 0
                            }
                            Label { text: fileName; color: theme.text; font.pixelSize: 13; elide: Text.ElideRight; Layout.preferredWidth: 240; Layout.fillWidth: true }
                            Label { text: formatTime(start); color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 104 }
                            Label { text: formatTime(end); color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 104 }
                            Label { text: formatTime(duration); color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 108 }
                            Label { text: delayBefore.toFixed(1) + "s"; color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 78 }
                            Label { text: audioEnabled ? "on" : "off"; color: audioEnabled ? theme.success : theme.danger; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 70 }
                            Label { text: Math.round(volume * 100) + "%"; color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 78 }
                            Label { text: speed.toFixed(2) + "x"; color: theme.bodyText; font.pixelSize: 13; font.family: "monospace"; Layout.preferredWidth: 72 }
                        }

                        HoverHandler {
                            id: rowHover
                            cursorShape: Qt.PointingHandCursor
                            onHoveredChanged: {
                                if (hovered)
                                    selectFragment(fragmentRow.index)
                            }
                        }

                        TapHandler {
                            onTapped: selectFragment(fragmentRow.index)
                            onDoubleTapped: {
                                selectFragment(fragmentRow.index)
                                previewSelectedDraft()
                            }
                        }

                    }
                }

            }
        }

        Rectangle {
            id: statusBar
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: leftPane.width
            height: 38
            radius: 12
            color: theme.block
            border.color: theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Label {
                    id: statusText
                    property var statusState: currentStatusBar()
                    text: statusState.text
                    color: statusState.error ? theme.danger : theme.bodyText
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    visible: text.length > 0
                }
            }
        }

        Rectangle {
            x: leftPane.width
            y: 0
            width: 14
            height: parent.height
            color: theme.appBg
        }

        Pane {
            id: rightPane
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.round((parent.width - 14) * 0.27)
            padding: 0
            background: Rectangle { color: "transparent" }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width * 9 / 16 + 216
                    Layout.minimumHeight: 390
                    color: theme.block
                    radius: 14
                    border.color: theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "Source Preview"
                                color: theme.text
                                font.pixelSize: 16
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            IconButton {
                                iconName: "open"
                                accentOnHover: true
                                toolTip: "Open selected source in video window"
                                enabled: selectedIndex >= 0 && draftValid
                                onClicked: openSelectedInPlaybackWindow()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width * 9 / 16
                            color: theme.videoBg
                            radius: 12
                            border.color: theme.border

                            VideoOutput {
                                id: videoOutput
                                anchors.fill: parent
                                anchors.margins: 1
                                fillMode: VideoOutput.PreserveAspectFit
                                Component.onCompleted: playback.setVideoSink(videoSink)
                            }

                            DelayPreviewOverlay {
                                anchors.fill: parent
                                anchors.margins: 1
                                active: playback.delayActive
                                delayColor: playback.currentDelayColor
                                remainingMs: playback.delayRemainingMs
                                progress: playback.delayProgress
                                compact: true
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: playlistModel.count === 0 && !playback.delayActive
                                text: "No media"
                                color: theme.mutedText
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppButton {
                                text: "Play"
                                primary: true
                                enabled: selectedIndex >= 0 && draftValid
                                onClicked: {
                                    playback.setVideoSink(videoOutput.videoSink)
                                    playback.previewRange(playlistView.currentIndex >= 0 ? playlistView.currentIndex : 0,
                                                          draftStartMs / 1000,
                                                          draftEndMs / 1000,
                                                          draftDelayMs / 1000,
                                                          draftDelayColor,
                                                          draftAudioEnabled,
                                                          draftVolume,
                                                          draftSpeed)
                                }
                                Layout.fillWidth: true
                            }

                            AppButton {
                                text: playback.playing ? "Pause" : (playback.currentIndex >= 0 ? "Play" : "Pause")
                                enabled: playback.currentIndex >= 0
                                onClicked: playback.playing ? playback.pause() : playback.resume()
                            }
                            AppButton { text: "Stop"; enabled: playback.currentIndex >= 0; onClicked: playback.stop() }
                        }

                        TrimTimeline {
                            Layout.fillWidth: true
                            enabled: selectedIndex >= 0 && trimTimelineDurationMs > 1
                            durationMs: trimTimelineDurationMs
                            startMs: draftStartMs
                            endMs: draftEndMs
                            positionMs: playback.currentIndex === selectedIndex ? playback.player.position : 0
                            onStartEdited: function(ms) {
                                draftStartMs = ms
                                draftDirty = true
                                seekSelectedSource(ms)
                            }
                            onEndEdited: function(ms) {
                                draftEndMs = ms
                                draftDirty = true
                                seekSelectedSource(ms)
                            }
                            onSeekRequested: function(ms) {
                                seekSelectedSource(ms)
                            }
                        }

                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.block
                    radius: 14
                    border.color: theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Label {
                            text: "Fragment Metadata"
                            color: theme.text
                            font.pixelSize: 16
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: theme.borderSoft
                        }

                        Flickable {
                            id: metadataScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: width
                            contentHeight: metadataGrid.implicitHeight
                            boundsBehavior: Flickable.StopAtBounds

                            GridLayout {
                                id: metadataGrid
                                width: metadataScroll.width
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 10

                            Label { text: "Source"; color: theme.bodyText }
                            Label {
                                text: selectedFragment.fileName || "-"
                                color: theme.bodyText
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Label { text: "Source duration"; color: theme.bodyText }
                            Label {
                                text: sourceDurationMs > 0 ? formatTimeMs(sourceDurationMs) : "-"
                                color: theme.bodyText
                                font.family: "monospace"
                                Layout.fillWidth: true
                            }

                            Label { text: "Source status"; color: theme.bodyText }
                            Label {
                                text: selectedFragment.sourceStatus || "-"
                                color: selectedFragment.sourceStatus === "Missing" ? theme.danger : theme.bodyText
                                Layout.fillWidth: true
                            }

                            Item { Layout.preferredHeight: 1 }
                            AppButton {
                                text: "Relink Source"
                                toolTip: "Relink Source"
                                enabled: selectedIndex >= 0
                                Layout.fillWidth: true
                                onClicked: relinkDialog.open()
                            }

                            Label { text: "Label"; color: theme.bodyText }
                            TextField {
                                text: draftLabel
                                enabled: selectedIndex >= 0
                                color: theme.text
                                selectedTextColor: "#ffffff"
                                selectionColor: theme.primary
                                placeholderText: selectedFragment.fileName || "Fragment label"
                                font.pixelSize: 13
                                implicitHeight: 34
                                onEditingFinished: {
                                    draftLabel = text.trim()
                                    draftDirty = true
                                }
                                Layout.fillWidth: true
                                background: Rectangle {
                                    radius: 10
                                    color: parent.activeFocus ? theme.blockAlt : theme.block
                                    border.color: parent.activeFocus ? theme.primary : theme.border
                                }
                            }

                            Label { text: "Notes"; color: theme.bodyText }
                            TextArea {
                                text: draftNotes
                                enabled: selectedIndex >= 0
                                color: theme.text
                                selectedTextColor: "#ffffff"
                                selectionColor: theme.primary
                                placeholderText: "Notes"
                                wrapMode: TextEdit.WordWrap
                                font.pixelSize: 13
                                implicitHeight: 72
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        draftNotes = text.trim()
                                        draftDirty = true
                                    }
                                }
                                Layout.fillWidth: true
                                background: Rectangle {
                                    radius: 10
                                    color: parent.activeFocus ? theme.blockAlt : theme.block
                                    border.color: parent.activeFocus ? theme.primary : theme.border
                                }
                            }

                        Label { text: "Start"; color: theme.bodyText }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                            TimeField {
                                valueMs: draftStartMs
                                enabled: selectedIndex >= 0
                                onCommitted: function(ms) {
                                    draftStartMs = ms
                                    draftDirty = true
                                }
                                Layout.fillWidth: true
                            }

                            AppButton {
                                text: "Set"
                                toolTip: "Set start from playhead"
                                enabled: selectedIndex >= 0
                                implicitWidth: 58
                                onClicked: {
                                    draftStartMs = Math.max(0, Math.round(playback.player.position))
                                    draftDirty = true
                                }
                            }
                        }

                        Label { text: "End"; color: theme.bodyText }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TimeField {
                                valueMs: draftEndMs
                                enabled: selectedIndex >= 0
                                onCommitted: function(ms) {
                                    draftEndMs = ms
                                    draftDirty = true
                                }
                                Layout.fillWidth: true
                            }

                            AppButton {
                                text: "Set"
                                toolTip: "Set end from playhead"
                                enabled: selectedIndex >= 0
                                implicitWidth: 58
                                onClicked: {
                                    draftEndMs = Math.round(playback.player.position)
                                    draftDirty = true
                                }
                            }
                        }

                        Label { text: "Delay"; color: theme.bodyText }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            TimeField {
                                valueMs: draftDelayMs
                                enabled: selectedIndex >= 0
                                onCommitted: function(ms) {
                                    draftDelayMs = ms
                                    draftDirty = true
                                }
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                id: delayColorSwatch
                                implicitWidth: 38
                                implicitHeight: 30
                                radius: 8
                                color: draftDelayColor
                                border.color: theme.border
                                border.width: 1

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: selectedIndex >= 0
                                    hoverEnabled: true
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Choose delay color"
                                    onClicked: {
                                        delayColorDialog.selectedColor = draftDelayColor
                                        delayColorDialog.open()
                                    }
                                }
                            }

                            Label {
                                text: draftDelayColor
                                visible: root.showDelayColorCode
                                color: theme.bodyText
                                font.family: "monospace"
                                Layout.preferredWidth: visible ? 78 : 0
                            }
                        }

                        Label { text: "Speed"; color: theme.bodyText }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppSlider {
                                id: speedSlider
                                enabled: selectedIndex >= 0
                                from: 0.25
                                to: 3.0
                                stepSize: 0.05
                                tickCount: 12
                                Component.onCompleted: value = draftSpeed
                                Connections {
                                    target: root
                                    function onDraftSpeedChanged() {
                                        if (!speedSlider.pressed)
                                            speedSlider.value = draftSpeed
                                    }
                                }
                                onMoved: {
                                    draftSpeed = value
                                    draftDirty = true
                                }
                                Layout.preferredWidth: 150
                                Layout.fillWidth: true
                            }

                            Label {
                                text: draftSpeed.toFixed(2) + "x"
                                color: theme.bodyText
                                font.family: "monospace"
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 48
                            }
                        }

                        Label { text: "Audio"; color: theme.bodyText }
                        AppSwitch {
                            enabled: selectedIndex >= 0
                            checked: draftAudioEnabled
                            onToggled: {
                                draftAudioEnabled = checked
                                draftDirty = true
                            }
                        }

                        Label { text: "Volume"; color: theme.bodyText }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppSlider {
                                id: volumeSlider
                                enabled: selectedIndex >= 0
                                from: 0
                                to: 1
                                stepSize: 0.01
                                tickCount: 5
                                Component.onCompleted: value = draftVolume
                                Connections {
                                    target: root
                                    function onDraftVolumeChanged() {
                                        if (!volumeSlider.pressed)
                                            volumeSlider.value = draftVolume
                                    }
                                }
                                onMoved: {
                                    draftVolume = value
                                    draftDirty = true
                                }
                                Layout.preferredWidth: 150
                                Layout.fillWidth: true
                            }

                            Label {
                                text: Math.round(draftVolume * 100) + "%"
                                color: theme.bodyText
                                font.family: "monospace"
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 48
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            Layout.preferredHeight: 1
                            color: theme.borderSoft
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            spacing: 8

                            Item { Layout.fillWidth: true }

                            IconButton {
                                iconName: "ios_share"
                                toolTip: "Export fragment"
                                accentOnHover: true
                                enabled: selectedIndex >= 0
                                onClicked: openFragmentExportDialog(selectedIndex)
                            }

                            IconButton {
                                iconName: "trash"
                                toolTip: "Remove"
                                enabled: selectedIndex >= 0
                                onClicked: removeSelectedFragment()
                            }

                            IconButton {
                                iconName: "copy"
                                toolTip: "Duplicate"
                                enabled: selectedIndex >= 0
                                onClicked: duplicateSelectedFragment()
                            }

                            IconButton {
                                iconName: "undo"
                                toolTip: "Reset"
                                enabled: selectedIndex >= 0 && draftDirty
                                onClicked: loadDraftFromSelection()
                            }

                            IconButton {
                                iconName: "save"
                                toolTip: "Save"
                                enabled: selectedIndex >= 0 && draftDirty && draftValid
                                onClicked: updateSelectedMetadata()
                            }
                        }

                    }
                }
            }
        }
    }
    }
    }

    component AboutPill: Rectangle {
        id: aboutPill
        property string text: ""

        implicitWidth: pillText.implicitWidth + 22
        implicitHeight: 28
        radius: 8
        color: "#26344d"
        border.color: "#3c4b68"

        Label {
            id: pillText
            anchors.centerIn: parent
            text: aboutPill.text
            color: "#e8eefb"
            font.pixelSize: 12
            font.bold: true
        }
    }

    component AboutCard: Rectangle {
        id: aboutCard
        property string title: ""
        default property alias content: aboutCardContent.data

        color: theme.block
        border.color: theme.border
        radius: 8
        implicitHeight: aboutCardLayout.implicitHeight + 36

        ColumnLayout {
            id: aboutCardLayout
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            Label {
                text: aboutCard.title
                color: theme.text
                font.pixelSize: 17
                font.bold: true
                Layout.fillWidth: true
            }

            ColumnLayout {
                id: aboutCardContent
                Layout.fillWidth: true
                spacing: 8
            }
        }
    }

    component InstructionRow: RowLayout {
        id: instructionRow
        property string number: ""
        property string text: ""

        Layout.fillWidth: true
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 8
            color: theme.primary

            Label {
                anchors.centerIn: parent
                text: instructionRow.number
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Label {
            text: instructionRow.text
            color: theme.bodyText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component ShortcutInfo: RowLayout {
        id: shortcutInfo
        property string keys: ""
        property string action: ""

        Layout.fillWidth: true
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 132
            Layout.preferredHeight: 30
            radius: 8
            color: theme.blockAlt
            border.color: theme.border

            Label {
                anchors.centerIn: parent
                text: shortcutInfo.keys
                color: theme.text
                font.family: "monospace"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                width: parent.width - 12
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Label {
            text: shortcutInfo.action
            color: theme.bodyText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component AppButton: Button {
        property bool primary: false
        property string toolTip: ""

        implicitHeight: 34
        implicitWidth: Math.max(86, contentItem.implicitWidth + 26)
        opacity: enabled ? 1.0 : 0.45
        hoverEnabled: true
        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.text: toolTip

        contentItem: Text {
            text: parent.text
            color: parent.primary ? "#ffffff" : theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
            font.bold: parent.primary
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 10
            color: parent.primary ? (parent.enabled && (parent.down || parent.hovered) ? theme.primaryStrong : theme.primary)
                                  : (parent.enabled && parent.down ? theme.blockAlt : (parent.enabled && parent.hovered ? theme.selected : theme.block))
            border.color: parent.primary || (parent.enabled && parent.hovered) ? theme.primary : theme.border

            Behavior on color { ColorAnimation { duration: 100 } }
            Behavior on border.color { ColorAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    component AppMenuAction: MenuItem {
        id: appMenuAction
        property string toolTip: ""

        width: parent ? parent.width : 260
        implicitWidth: 260
        implicitHeight: 34
        hoverEnabled: true
        opacity: 1.0

        contentItem: Text {
            text: appMenuAction.text
            color: appMenuAction.enabled ? theme.text : theme.disabled
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 13
            leftPadding: 12
            rightPadding: 12
        }

        background: Rectangle {
            radius: 8
            color: appMenuAction.enabled && (appMenuAction.hovered || appMenuAction.down) ? theme.selected : "transparent"
        }

        ToolTip.visible: appMenuAction.hovered && appMenuAction.toolTip.length > 0
        ToolTip.text: appMenuAction.toolTip

        HoverHandler {
            cursorShape: appMenuAction.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    component AppMenuSeparator: MenuSeparator {
        width: parent ? parent.width : 260
        implicitWidth: 260
        implicitHeight: 9

        contentItem: Rectangle {
            implicitHeight: 9
            color: "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                height: 1
                color: theme.borderSoft
            }
        }
    }

    component ProjectMenuItem: Item {
        id: projectMenuItem
        property string text: ""
        property string toolTip: ""
        signal triggered()

        width: parent ? parent.width : 260
        implicitWidth: 260
        implicitHeight: 34
        opacity: 1.0

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: projectMenuItem.enabled && (projectMenuMouse.containsMouse || projectMenuMouse.pressed) ? theme.selected : "transparent"
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            text: projectMenuItem.text
            color: projectMenuItem.enabled ? theme.text : theme.disabled
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 13
        }

        ToolTip.visible: projectMenuMouse.containsMouse && projectMenuItem.toolTip.length > 0
        ToolTip.text: projectMenuItem.toolTip

        MouseArea {
            id: projectMenuMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: projectMenuItem.enabled
            cursorShape: projectMenuItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: projectMenuItem.triggered()
        }
    }

    component ProjectMenuSeparator: Item {
        width: parent ? parent.width : 260
        implicitWidth: 260
        implicitHeight: 9

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 1
            color: theme.borderSoft
        }
    }

    component IconButton: Button {
        id: iconButton
        property string iconName: ""
        property string toolTip: ""
        property bool accentOnHover: false

        readonly property var iconGlyphs: ({
            "menu": "menu",
            "sun": "light_mode",
            "moon": "dark_mode",
            "info": "info",
            "open": "open_in_new",
            "link": "link",
            "trash": "delete",
            "copy": "content_copy",
            "undo": "undo",
            "save": "save",
            "palette": "palette",
            "arrow-up": "publish",
            "arrow-down": "arrow_downward",
            "ios_share": "ios_share"
        })

        implicitWidth: 38
        implicitHeight: 34
        hoverEnabled: true
        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.text: toolTip

        contentItem: Item {
            Label {
                anchors.centerIn: parent
                text: iconButton.iconGlyphs[iconButton.iconName] || "\u25A1"
                color: iconButton.enabled && iconButton.accentOnHover && (iconButton.hovered || iconButton.down) ? "#ffffff" : theme.text
                font.family: materialSymbols.name.length > 0 ? materialSymbols.name : "Noto Sans Symbols 2"
                font.pixelSize: 18
                font.bold: false
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: 20
                height: 20
            }
        }

        background: Rectangle {
            radius: 10
            color: iconButton.enabled && iconButton.accentOnHover && iconButton.down ? theme.primaryStrong
                   : (iconButton.enabled && iconButton.accentOnHover && iconButton.hovered ? theme.primary
                   : (iconButton.enabled && iconButton.down ? theme.blockAlt : (iconButton.enabled && iconButton.hovered ? theme.selected : theme.block)))
            border.color: iconButton.enabled && iconButton.hovered ? theme.primary : theme.border

            Behavior on color { ColorAnimation { duration: 100 } }
            Behavior on border.color { ColorAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            cursorShape: iconButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    component AppSlider: Slider {
        id: appSlider
        property int tickCount: 0

        implicitHeight: 26
        hoverEnabled: true

        background: Item {
            x: appSlider.leftPadding
            y: appSlider.topPadding + appSlider.availableHeight / 2 - 2
            width: appSlider.availableWidth
            height: 4

            Rectangle {
                anchors.fill: parent
                radius: 2
                color: theme.rail
            }

            Rectangle {
                width: appSlider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: theme.primaryStrong
            }

            Repeater {
                model: appSlider.tickCount
                Rectangle {
                    width: 5
                    height: 5
                    radius: 2.5
                    x: appSlider.tickCount <= 1 ? parent.width / 2 - width / 2 : (index / (appSlider.tickCount - 1)) * (parent.width - width)
                    y: parent.height / 2 - height / 2
                    color: appSlider.tickCount <= 1 || index / (appSlider.tickCount - 1) <= appSlider.visualPosition ? theme.primaryDark : theme.disabled
                }
            }
        }

        handle: Rectangle {
            x: appSlider.leftPadding + appSlider.visualPosition * (appSlider.availableWidth - width)
            y: appSlider.topPadding + appSlider.availableHeight / 2 - height / 2
            width: 18
            height: 18
            radius: 9
            color: theme.block
            border.width: 4
            border.color: appSlider.enabled ? theme.primaryStrong : theme.disabled

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: appSlider.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    component AppSwitch: Switch {
        id: appSwitch

        implicitWidth: 42
        implicitHeight: 24
        text: checked ? "On" : "Off"
        spacing: 8
        opacity: enabled ? 1.0 : 0.45

        indicator: Rectangle {
            implicitWidth: 32
            implicitHeight: 18
            x: appSwitch.leftPadding
            y: parent.height / 2 - height / 2
            radius: 9
            color: appSwitch.checked ? theme.primary : theme.block
            border.color: appSwitch.checked ? theme.primary : theme.border

            Rectangle {
                width: 14
                height: 14
                radius: 7
                x: appSwitch.checked ? parent.width - width - 2 : 2
                y: 2
                color: appSwitch.checked ? "#ffffff" : theme.mutedText
            }

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        contentItem: Text {
            text: appSwitch.text
            leftPadding: appSwitch.indicator.width + appSwitch.spacing
            verticalAlignment: Text.AlignVCenter
            color: theme.bodyText
            font.pixelSize: 13
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: appSwitch.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    component TrimTimeline: Item {
        id: trimTimeline
        property int durationMs: 0
        property int startMs: 0
        property int endMs: 0
        property int positionMs: 0
        property bool controlsVisible: enabled && (timelineHover.containsMouse || startDrag.pressed || endDrag.pressed)
        signal startEdited(int valueMs)
        signal endEdited(int valueMs)
        signal seekRequested(int valueMs)

        implicitHeight: 62
        opacity: enabled ? 1.0 : 0.45

        MouseArea {
            id: timelineHover
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        Rectangle {
            id: trimTrack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            y: 22
            height: 12
            radius: 6
            color: theme.rail
            border.color: theme.borderSoft

            MouseArea {
                anchors.fill: parent
                enabled: trimTimeline.enabled
                cursorShape: Qt.PointingHandCursor
                onPressed: trimTimeline.seekRequested(trimTimeline.msForTrackX(mouse.x))
                onPositionChanged: {
                    if (pressed)
                        trimTimeline.seekRequested(trimTimeline.msForTrackX(mouse.x))
                }
            }

            Rectangle {
                x: trimTimeline.xForMs(trimTimeline.startMs)
                width: Math.max(2, trimTimeline.xForMs(trimTimeline.endMs) - x)
                height: parent.height
                radius: 6
                color: theme.primary
                opacity: 0.78
            }

            Rectangle {
                x: trimTimeline.xForMs(trimTimeline.positionMs) - width / 2
                y: -8
                width: 3
                height: 28
                radius: 1.5
                color: theme.primaryDark
                opacity: trimTimeline.controlsVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }

        Rectangle {
            id: startHandle
            x: trimTrack.x + trimTimeline.xForMs(trimTimeline.startMs) - width / 2
            y: trimTrack.y - 9
            width: 16
            height: 30
            radius: 5
            color: startDrag.pressed ? theme.primaryStrong : theme.block
            border.width: 2
            border.color: theme.primary
            opacity: trimTimeline.controlsVisible ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: 100 } }

            MouseArea {
                id: startDrag
                anchors.fill: parent
                enabled: trimTimeline.controlsVisible
                cursorShape: Qt.SizeHorCursor
                preventStealing: true
                onPositionChanged: {
                    if (!pressed)
                        return

                    const point = mapToItem(trimTrack, mouse.x, mouse.y)
                    const value = Math.min(trimTimeline.endMs - 1, trimTimeline.msForTrackX(point.x))
                    trimTimeline.startEdited(Math.max(0, value))
                }
            }
        }

        Rectangle {
            id: endHandle
            x: trimTrack.x + trimTimeline.xForMs(trimTimeline.endMs) - width / 2
            y: trimTrack.y - 9
            width: 16
            height: 30
            radius: 5
            color: endDrag.pressed ? theme.primaryStrong : theme.block
            border.width: 2
            border.color: theme.primary
            opacity: trimTimeline.controlsVisible ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: 100 } }

            MouseArea {
                id: endDrag
                anchors.fill: parent
                enabled: trimTimeline.controlsVisible
                cursorShape: Qt.SizeHorCursor
                preventStealing: true
                onPositionChanged: {
                    if (!pressed)
                        return

                    const point = mapToItem(trimTrack, mouse.x, mouse.y)
                    const value = Math.max(trimTimeline.startMs + 1, trimTimeline.msForTrackX(point.x))
                    trimTimeline.endEdited(Math.min(trimTimeline.durationMs, value))
                }
            }
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: trimTrack.bottom
            anchors.topMargin: 10
            spacing: 8

            Label {
                text: "Start: " + formatTimeMs(trimTimeline.startMs)
                color: theme.bodyText
                font.pixelSize: 12
                font.family: "monospace"
                horizontalAlignment: Text.AlignLeft
                Layout.preferredWidth: 140
            }

            Item { Layout.fillWidth: true }

            Label {
                text: "End: " + formatTimeMs(trimTimeline.endMs)
                color: theme.bodyText
                font.pixelSize: 12
                font.family: "monospace"
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 140
            }
        }

        function xForMs(valueMs) {
            if (durationMs <= 0 || trimTrack.width <= 0)
                return 0

            const clamped = Math.max(0, Math.min(durationMs, Math.round(valueMs || 0)))
            return clamped / durationMs * trimTrack.width
        }

        function msForTrackX(valueX) {
            if (durationMs <= 0 || trimTrack.width <= 0)
                return 0

            const clampedX = Math.max(0, Math.min(trimTrack.width, valueX))
            return Math.round(clampedX / trimTrack.width * durationMs)
        }
    }

    component TimeField: TextField {
        id: timeField
        property int valueMs: 0
        signal committed(int valueMs)

        implicitHeight: 34
        text: formatTimeMs(valueMs)
        color: theme.text
        selectedTextColor: "#ffffff"
        selectionColor: theme.primary
        horizontalAlignment: TextInput.AlignLeft
        verticalAlignment: TextInput.AlignVCenter
        font.family: "monospace"
        font.pixelSize: 13
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        opacity: enabled ? 1.0 : 0.45

        onValueMsChanged: {
            if (!activeFocus)
                text = formatTimeMs(valueMs)
        }

        onEditingFinished: commitText()

        background: Rectangle {
            radius: 10
            color: timeField.activeFocus ? theme.blockAlt : theme.block
            border.color: timeField.activeFocus ? theme.primary : theme.border
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: timeField.enabled ? Qt.IBeamCursor : Qt.ArrowCursor
        }

        function commitText() {
            const parsed = parseTimeMs(text)
            text = formatTimeMs(parsed)
            committed(parsed)
        }
    }

    component DelayPreviewOverlay: Item {
        id: delayOverlay
        property bool active: false
        property string delayColor: "#000000"
        property int remainingMs: 0
        property real progress: 0.0
        property bool compact: false

        visible: opacity > 0
        opacity: active ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Rectangle {
            anchors.fill: parent
            color: delayOverlay.delayColor
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: delayOverlay.compact ? 42 : 58
            color: "#99000000"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: delayOverlay.compact ? 12 : 18
                anchors.rightMargin: delayOverlay.compact ? 12 : 18
                spacing: 12

                Label {
                    text: "Delay"
                    color: "#ffffff"
                    font.pixelSize: delayOverlay.compact ? 13 : 16
                    font.bold: true
                    Layout.preferredWidth: delayOverlay.compact ? 54 : 72
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: delayOverlay.compact ? 7 : 9
                    radius: height / 2
                    color: "#55ffffff"
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * Math.max(0, Math.min(1, delayOverlay.progress))
                        radius: parent.radius
                        color: "#ffffff"
                    }
                }

                Label {
                    text: formatTimeMs(delayOverlay.remainingMs)
                    color: "#ffffff"
                    font.family: "monospace"
                    font.pixelSize: delayOverlay.compact ? 12 : 14
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: delayOverlay.compact ? 76 : 90
                }
            }
        }
    }

    function formatTime(seconds) {
        return formatTimeMs(Math.round((seconds || 0) * 1000))
    }

    function formatTimeMs(milliseconds) {
        const totalMs = Math.max(0, Math.round(milliseconds || 0))
        const totalSeconds = Math.floor(totalMs / 1000)
        const h = Math.floor(totalSeconds / 3600)
        const m = Math.floor((totalSeconds % 3600) / 60)
        const s = totalSeconds % 60
        const ms = totalMs % 1000
        const suffix = "." + ms.toString().padStart(3, "0")
        if (h > 0)
            return h.toString().padStart(2, "0") + ":" + m.toString().padStart(2, "0") + ":" + s.toString().padStart(2, "0") + suffix
        return m.toString().padStart(2, "0") + ":" + s.toString().padStart(2, "0") + suffix
    }

    function formatDateTime(value) {
        if (!value)
            return "-"

        const date = new Date(value)
        if (Number.isNaN(date.getTime()))
            return "-"

        return date.toLocaleString(Qt.locale(), Locale.ShortFormat)
    }

    function parseTimeMs(value) {
        const cleaned = String(value || "").trim().toLowerCase().replace("s", "")
        if (cleaned.length === 0)
            return 0

        const parts = cleaned.split(":")
        let seconds = 0

        if (parts.length === 1) {
            seconds = Number(parts[0])
        } else if (parts.length === 2) {
            seconds = Number(parts[0]) * 60 + Number(parts[1])
        } else {
            seconds = Number(parts[parts.length - 3]) * 3600
                    + Number(parts[parts.length - 2]) * 60
                    + Number(parts[parts.length - 1])
        }

        if (!Number.isFinite(seconds))
            return 0

        return Math.max(0, Math.round(seconds * 1000))
    }

    function colorToHex(value) {
        const text = String(value || "")
        const rgb = text.match(/^#[0-9a-fA-F]{6}$/)
        if (rgb)
            return rgb[0].toLowerCase()

        const argb = text.match(/^#[0-9a-fA-F]{8}$/)
        return argb ? ("#" + text.slice(3)).toLowerCase() : "#000000"
    }

    function loadDraftFromSelection() {
        if (selectedIndex < 0) {
            draftStartMs = 0
            draftEndMs = 0
            draftDelayMs = 0
            draftDelayColor = "#000000"
            draftSpeed = 1.0
            draftAudioEnabled = true
            draftVolume = 1.0
            draftLabel = ""
            draftNotes = ""
            draftDirty = false
            return
        }

        const fragment = playlistModel.get(selectedIndex)
        draftStartMs = Math.round((fragment.start || 0) * 1000)
        draftEndMs = Math.round((fragment.end || 0) * 1000)
        draftDelayMs = Math.round((fragment.delayBefore || 0) * 1000)
        draftDelayColor = colorToHex(fragment.delayColor || "#000000")
        draftSpeed = fragment.speed || 1.0
        draftAudioEnabled = fragment.audioEnabled !== false
        draftVolume = fragment.volume || 1.0
        draftLabel = fragment.label || ""
        draftNotes = fragment.notes || ""
        draftDirty = false
    }

    function selectFragment(index) {
        if (index < 0 || index >= playlistModel.count)
            return

        if (playlistView.currentIndex !== index) {
            playlistView.currentIndex = index
        } else {
            loadDraftFromSelection()
            cueSelectionForPreview()
        }
    }

    function cueSelectionForPreview() {
        if (selectedIndex < 0 || playback.playing)
            return

        playback.setVideoSink(videoOutput.videoSink)
        playback.cue(selectedIndex)
    }

    function previewSelectedDraft() {
        if (selectedIndex < 0 || !draftValid)
            return

        playback.setVideoSink(videoOutput.videoSink)
        playback.previewRange(selectedIndex,
                              draftStartMs / 1000,
                              draftEndMs / 1000,
                              draftDelayMs / 1000,
                              draftDelayColor,
                              draftAudioEnabled,
                              draftVolume,
                              draftSpeed)
    }

    function validateDraft() {
        if (selectedIndex < 0)
            return ""

        if (selectedFragment.sourceStatus === "Missing")
            return "Source file does not exist."

        if (draftEndMs <= draftStartMs)
            return "End time must be greater than start time."

        if (sourceDurationMs > 0 && draftStartMs >= sourceDurationMs)
            return "Start time is outside the source media duration."

        if (sourceDurationMs > 0 && draftEndMs > sourceDurationMs)
            return "End time is outside the source media duration."

        return ""
    }

    function updateSelectedMetadata() {
        if (selectedIndex < 0)
            return

        playlistModel.updateFragment(selectedIndex, {
            "start": draftStartMs / 1000,
            "end": draftEndMs / 1000,
            "delayBefore": draftDelayMs / 1000,
            "delayColor": draftDelayColor,
            "speed": draftSpeed,
            "audioEnabled": draftAudioEnabled,
            "volume": draftVolume,
            "label": draftLabel,
            "notes": draftNotes
        })
        loadDraftFromSelection()
    }

    function openPlaylistFile(fileUrl) {
        if (!fileUrl || fileUrl.toString().length === 0) {
            setStatusBarMessage("No playlist file selected.", true)
            return false
        }

        setStatusBarMessage("Opening " + fileUrl.toString(), false)

        let loaded = false
        try {
            loaded = playlistModel.load(fileUrl)
        } catch (error) {
            setStatusBarMessage(String(error), true)
            return false
        }

        if (!loaded) {
            const localPath = localPathFromFileUrl(fileUrl)
            if (localPath.length > 0 && localPath !== fileUrl.toString()) {
                setStatusBarMessage("Retrying as " + localPath, false)
                try {
                    loaded = playlistModel.load(localPath)
                } catch (error) {
                    setStatusBarMessage(String(error), true)
                    return false
                }
            }
        }

        if (loaded) {
            try {
                addRecentPlaylist(fileUrl)
                playlistView.currentIndex = playlistModel.count > 0 ? 0 : -1
                loadDraftFromSelection()
                setStatusBarMessage("Opened " + playlistModel.name + " (" + playlistModel.count + " fragments).", false)
            } catch (error) {
                setStatusBarMessage(String(error), true)
                return false
            }
            return true
        }

        removeRecentPlaylist(fileUrl)
        setStatusBarMessage("Could not open " + fileUrl.toString(), true)
        return false
    }

    function savePlaylistFile(fileUrl) {
        if (!fileUrl || fileUrl.toString().length === 0)
            return false

        if (playlistModel.save(fileUrl)) {
            addRecentPlaylist(fileUrl)
            return true
        }

        return false
    }

    function savePlaylist() {
        if (playlistModel.fileUrl && playlistModel.fileUrl.toString().length > 0) {
            savePlaylistFile(playlistModel.fileUrl)
        } else {
            openSaveDialog()
        }
    }

    function savePlaylistAndContinue() {
        if (playlistModel.fileUrl && playlistModel.fileUrl.toString().length > 0) {
            if (savePlaylistFile(playlistModel.fileUrl))
                continuePendingAction()
        } else {
            continueAfterSave = true
            openSaveDialog()
        }
    }

    function openSaveDialog() {
        const suggestedFileName = playlistModel.suggestedFileName.length > 0 ? playlistModel.suggestedFileName : "untitled.json"
        let folder = saveDialog.currentFolder
        if (!folder || folder.toString().length === 0) {
            folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
            saveDialog.currentFolder = folder
        }

        let folderUrl = folder.toString()
        if (!folderUrl.endsWith("/"))
            folderUrl += "/"

        const suggestedFileUrl = folderUrl + suggestedFileName
        saveDialog.currentFile = suggestedFileUrl
        saveDialog.selectedFile = suggestedFileUrl
        saveDialog.open()
    }

    function addRecentPlaylist(fileUrl) {
        const url = fileUrl.toString()
        if (url.length === 0)
            return

        const next = [url]
        for (let i = 0; i < recentPlaylistUrls.length && next.length < 8; ++i) {
            const existing = recentPlaylistUrls[i].toString()
            if (existing !== url)
                next.push(existing)
        }

        recentPlaylistUrls = next
        appSettings.recentPlaylistUrls = next
    }

    function removeRecentPlaylist(fileUrl) {
        const url = fileUrl.toString()
        const next = []
        for (let i = 0; i < recentPlaylistUrls.length; ++i) {
            const existing = recentPlaylistUrls[i].toString()
            if (existing !== url)
                next.push(existing)
        }

        recentPlaylistUrls = next
        appSettings.recentPlaylistUrls = next
    }

    function clearRecentPlaylists() {
        recentPlaylistUrls = []
        appSettings.recentPlaylistUrls = []
    }

    function recentPlaylistLabel(fileUrl) {
        const text = fileUrl.toString()
        const slash = text.lastIndexOf("/")
        if (slash >= 0 && slash + 1 < text.length)
            return decodeURIComponent(text.substring(slash + 1))
        return text
    }

    function openSelectedInPlaybackWindow() {
        if (selectedIndex < 0 || !draftValid)
            return

        playbackWindow.show()
        playbackWindow.raise()
        playbackWindow.requestActivate()
        playback.setVideoSink(playbackWindowVideo.videoSink)
        playback.previewRange(selectedIndex,
                              draftStartMs / 1000,
                              draftEndMs / 1000,
                              draftDelayMs / 1000,
                              draftDelayColor,
                              draftAudioEnabled,
                              draftVolume,
                              draftSpeed)
    }

    function requestOpenPlaylist() {
        if (playlistModel.modified) {
            pendingAction = "open"
            unsavedDialog.show()
            return
        }

        Qt.callLater(function() {
            openDialog.open()
        })
    }

    function requestOpenRecentPlaylist(fileUrl) {
        pendingRecentPlaylistUrl = fileUrl
        if (playlistModel.modified) {
            pendingAction = "openRecent"
            unsavedDialog.show()
            return
        }

        openPlaylistFile(fileUrl)
    }

    function requestNewPlaylist() {
        if (playlistModel.modified) {
            pendingAction = "new"
            unsavedDialog.show()
            return
        }

        createNewPlaylist()
    }

    function continuePendingAction() {
        const action = pendingAction
        pendingAction = ""

        if (action === "open") {
            openDialog.open()
        } else if (action === "openRecent") {
            openPlaylistFile(pendingRecentPlaylistUrl)
            pendingRecentPlaylistUrl = ""
        } else if (action === "new") {
            createNewPlaylist()
        } else if (action === "close") {
            forceClose = true
            root.close()
        }
    }

    function createNewPlaylist() {
        playback.stop()
        playlistModel.newPlaylist()
        playlistView.currentIndex = -1
        loadDraftFromSelection()
    }

    function duplicateSelectedFragment() {
        if (selectedIndex < 0)
            return

        const duplicateRow = playlistModel.duplicateFragment(selectedIndex)
        if (duplicateRow >= 0) {
            playlistView.currentIndex = duplicateRow
            loadDraftFromSelection()
        }
    }

    function removeSelectedFragment() {
        const row = selectedIndex
        if (row < 0)
            return

        playlistModel.removeFragment(row)
        if (playlistModel.count === 0) {
            playlistView.currentIndex = -1
        } else {
            playlistView.currentIndex = Math.min(row, playlistModel.count - 1)
        }
        loadDraftFromSelection()
    }

    function undoPlaylist() {
        if (!playlistModel.canUndo)
            return

        const previousIndex = selectedIndex
        playlistModel.undo()
        playlistView.currentIndex = playlistModel.count === 0 ? -1 : Math.max(0, Math.min(previousIndex, playlistModel.count - 1))
        loadDraftFromSelection()
        cueSelectionForPreview()
    }

    function redoPlaylist() {
        if (!playlistModel.canRedo)
            return

        const previousIndex = selectedIndex
        playlistModel.redo()
        playlistView.currentIndex = playlistModel.count === 0 ? -1 : Math.max(0, Math.min(previousIndex, playlistModel.count - 1))
        loadDraftFromSelection()
        cueSelectionForPreview()
    }

    function moveSelectedFragment(direction) {
        const from = playlistView.currentIndex
        const to = from + direction
        moveFragmentTo(from, to)
    }

    function moveFragmentTo(from, to) {
        if (from < 0 || to < 0 || to >= playlistModel.count || from === to)
            return

        playlistModel.moveFragment(from, to)
        playlistView.currentIndex = to
    }

    function setDraftStartFromCurrent() {
        if (selectedIndex < 0)
            return

        draftStartMs = Math.max(0, Math.round(playback.player.position))
        draftDirty = true
    }

    function setDraftEndFromCurrent() {
        if (selectedIndex < 0)
            return

        draftEndMs = Math.max(0, Math.round(playback.player.position))
        draftDirty = true
    }

    function seekSelectedSource(positionMs) {
        if (selectedIndex < 0)
            return

        playback.setVideoSink(videoOutput.videoSink)
        if (playback.currentIndex !== selectedIndex || playback.playing)
            playback.cue(selectedIndex)

        playback.player.position = Math.max(0, Math.round(positionMs || 0))
    }

    function togglePlaybackFullscreen() {
        if (playbackWindow.visibility === Window.FullScreen)
            playbackWindow.showNormal()
        else
            playbackWindow.showFullScreen()
    }

    function showPlaybackControls() {
        playbackWindow.controlsVisible = true
        playbackControlsHideTimer.stop()
        if (playbackWindow.visible && playbackWindow.visibility === Window.FullScreen && playback.playing)
            playbackControlsHideTimer.start()
    }
}
