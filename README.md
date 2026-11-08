# Apexion Linux 0.1 (Debian trixie)

Live-ISO на основе Debian 13 (trixie), x86_64.

## Что внутри

- **systemd** как init (стандарт Debian)
- **XFCE** с LightDM и NetworkManager, автологин пользователя `user`
- **Загрузка:** BIOS через syslinux, UEFI через GRUB. Всё в одном гибридном ISO.
- **apx**: пакетный менеджер на C++17, фронтенд над apt
- **fastfetch** с логотипом Apexion

## Сборка в облаке (GitHub Actions, можно с телефона)

1. Создай приватный репозиторий на github.com и загрузи в него содержимое папки проекта, включая скрытую `.github`.
2. Открой **Actions** → **Build Apexion Linux ISO (Debian)** → **Run workflow**.
3. Жди 1–2 часа.
4. Скачай артефакт `apexion-linux-0.1-amd64` из раздела Artifacts внизу запуска.

## Сборка локально (Debian trixie, от root)

```
sudo apt install live-build g++ dpkg-dev xorriso squashfs-tools \
                 grub-pc-bin grub-efi-amd64-bin grub-common mtools dosfstools
sudo bash build-debian.sh
```

Результат: `apexion-linux-0.1-amd64.iso`.

## Структура

```
apexion-linux-debian/
├── build-debian.sh                   сборка apx.deb и ISO
├── apx/src/main.cpp                  пакетный менеджер (apt)
├── .github/workflows/                сборка в GitHub Actions
└── live-build/
    ├── auto/config                   параметры образа
    └── config/
        ├── package-lists/            список пакетов
        ├── hooks/live/               хуки: брендинг и меню загрузчика
        └── includes.chroot/          файлы, которые попадают в систему
```

## Что важно

- Образ будет весить несколько гигабайт, и загрузка на телефон займёт время. Лучше скачивать по Wi-Fi.
- Артефакт хранится 7 дней.
- Файлы, загруженные через браузер, теряют бит исполнения. Скрипт сам восстанавливает его для `auto/config` и хуков.
- Автологин включён для live-режима. Перед установкой на диск убери `50-apexion.conf`.
- Проверено только по документации: сборку ISO в этой среде не запускал. Первая сборка может упасть. Смотри первую строку с ошибкой в логе.
