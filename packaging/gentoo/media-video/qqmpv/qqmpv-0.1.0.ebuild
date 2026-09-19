# Copyright 2026 QindaQt contributors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

# git-r3 pulls the pinned release commit from the local development checkout.
# AGENT-NOTE: once the repository is hosted, replace this with a standard
# SRC_URI tarball (the CMake layout stays identical); bump the version and
# commit together and regenerate the Manifest.
inherit cmake xdg git-r3

DESCRIPTION="QindaMPV - mpv media player with QindaTK UI and global menu for the QindaQt desktop"
HOMEPAGE="https://github.com/Es00bac/QindaQt"
EGIT_REPO_URI="file:///home/cabewse/work_space/QindaMPV"
# v0.1.0 — pin the release commit here at release time.
EGIT_COMMIT="9fcda3f4e950d0a89297c414405cc97b4a70ac36"

LICENSE="GPL-3+"
SLOT="0"
KEYWORDS="~amd64"
IUSE=""

# gui-wm/qindaqt-desktop owns the QindaQt AppShell and global-menu archives
# and headers this app links (no CMake package configs are installed for
# them, hence the find_library use in CMakeLists.txt). libmpv is loaded at
# run time: the UI works without it, playback needs media-video/mpv.
RDEPEND="
	>=gui-wm/qindaqt-desktop-0.1.0_pre20260907-r1
	>=dev-qt/qtbase-6.11:6=[dbus,gui,opengl,wayland,widgets]
	>=dev-qt/qtdeclarative-6.11:6=
	dev-libs/qindatk
	media-video/mpv[libmpv]
"
DEPEND="${RDEPEND}"
BDEPEND="
	virtual/pkgconfig
"

src_configure() {
	local mycmakeargs=(
		-DBUILD_TESTING=OFF
	)
	cmake_src_configure
}
