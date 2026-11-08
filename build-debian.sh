#!/bin/bash
# Сборка Apexion Linux 0.1 на Debian trixie: собирает apx.deb и live-ISO через live-build.
# Запуск от root: sudo bash build-debian.sh (или внутри контейнера debian:trixie).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="0.1"
APX_VERSION="0.1.0"
LB="$ROOT/live-build"
ISO_OUT="$ROOT/apexion-linux-${VERSION}-amd64.iso"

if [ "$(id -u)" -ne 0 ]; then
    echo "Запусти от root: sudo bash $0" >&2
    exit 1
fi

for cmd in g++ dpkg-deb lb xorriso mksquashfs; do
    command -v "$cmd" >/dev/null 2>&1 || { echo "Нет команды: $cmd" >&2; exit 1; }
done

# Файлы, загруженные через браузер или телефон, теряют бит исполнения: возвращаем его
chmod +x "$LB/auto/config"
find "$LB/config/hooks" -type f -exec chmod +x {} +

echo "==> 1/3 сборка apx"
mkdir -p "$ROOT/apx/build"
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o "$ROOT/apx/build/apx" "$ROOT/apx/src/main.cpp"

echo "==> 2/3 упаковка apx в .deb для образа"
PKG="$ROOT/apx/build/pkg"
rm -rf "$PKG"
install -d "$PKG/DEBIAN" "$PKG/usr/bin" "$LB/config/packages.chroot"
install -m 0755 "$ROOT/apx/build/apx" "$PKG/usr/bin/apx"
cat > "$PKG/DEBIAN/control" <<EOF
Package: apx
Version: ${APX_VERSION}
Architecture: amd64
Maintainer: Apexion Team
Section: admin
Priority: optional
Depends: apt, sudo
Description: apx — пакетный менеджер Apexion Linux
 Удобный фронтенд над apt: установка, удаление, поиск и обновление пакетов.
EOF
dpkg-deb --build --root-owner-group "$PKG" "$LB/config/packages.chroot/apx_${APX_VERSION}_amd64.deb"

echo "==> 3/3 сборка live-ISO (долго, нужен интернет)"
cd "$LB"
lb clean --purge >/dev/null 2>&1 || true
lb config
lb build
mv live-image-*.iso "$ISO_OUT"

echo "==> Готово: $ISO_OUT"
