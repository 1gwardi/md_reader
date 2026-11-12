#!/bin/zsh
# Install macOS prerequisites, build mdread, and install it for the current user.
emulate -LR zsh
setopt ERR_EXIT NO_UNSET PIPE_FAIL

usage() {
  cat <<'HELP'
Использование: ./mac_setup.zsh [опции]
  --prefix ПУТЬ     Куда установить mdread (по умолчанию ~/.local)
  --build-dir ПУТЬ Каталог сборки (по умолчанию <проект>/build/macos)
  --build-only     Установить зависимости и собрать без установки mdread
  -h, --help       Справка

Нужен macOS. Отсутствующие зависимости устанавливаются через Homebrew.
Если нет Command Line Tools, откроется установщик Apple: после его завершения
запустите скрипт ещё раз. Homebrew при первой установке может запросить пароль.
HELP
}

fail() { print -u2 -r -- "mac_setup: $*"; exit 1; }
step() { print -r -- "==> $*"; }

readonly project_root=${0:A:h}
build_dir="$project_root/build/macos"
install_prefix="$HOME/.local"
install_binary=true

while (( $# )); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --prefix|--build-dir)
      (( $# >= 2 )) || fail "После $1 нужен путь"
      [[ -n "$2" && "$2" != --* ]] || fail "После $1 нужен путь"
      if [[ "$1" == --prefix ]]; then install_prefix=${2:A}; else build_dir=${2:A}; fi
      shift 2 ;;
    --build-only) install_binary=false; shift ;;
    *) fail "Неизвестная опция: $1 (см. --help)" ;;
  esac
done

[[ "$(uname -s)" == Darwin ]] || fail "Этот скрипт предназначен для macOS"
(( EUID != 0 )) || fail "Запустите скрипт от обычного пользователя, без sudo"
[[ -f "$project_root/CMakeLists.txt" ]] || fail "Рядом со скриптом не найден CMakeLists.txt"
[[ "$build_dir" != "$project_root" ]] || fail "Каталог сборки должен отличаться от каталога исходников"
trap 'print -u2 -- "mac_setup: шаг завершился с ошибкой (строка $LINENO)."' ZERR

step "Проверка Command Line Tools и macOS SDK"
if ! xcrun --find clang++ >/dev/null 2>&1 || ! xcrun --show-sdk-path >/dev/null 2>&1; then
  xcode-select --install || fail "Не удалось открыть установщик Apple; проверьте xcode-select --install"
  print -r -- "Завершите установку Command Line Tools в окне Apple и запустите скрипт ещё раз."
  exit 1
fi

find_brew() {
  if (( $+commands[brew] )); then
    brew_bin=$commands[brew]
  elif [[ -x /opt/homebrew/bin/brew ]]; then
    brew_bin=/opt/homebrew/bin/brew
  elif [[ -x /usr/local/bin/brew ]]; then
    brew_bin=/usr/local/bin/brew
  else
    return 1
  fi
}

brew_bin=''
if ! find_brew; then
  step "Установка Homebrew через официальный установщик"
  installer_file=$(mktemp "${TMPDIR:-/tmp}/mdread-homebrew.XXXXXX")
  trap '[[ -z "$installer_file" ]] || /bin/rm -f -- "$installer_file"' EXIT
  curl --fail --silent --show-error --location --retry 3 \
    https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh --output "$installer_file"
  /bin/bash "$installer_file"
  rehash
  find_brew || fail "Homebrew не найден после установки"
fi

brew_environment=$("$brew_bin" shellenv)
eval "$brew_environment"
export HOMEBREW_NO_AUTO_UPDATE=1
step "Проверка зависимостей: CMake, pkg-config, MD4C"
missing=()
for dependency in cmake pkgconf md4c; do
  if ! "$brew_bin" list --versions "$dependency" >/dev/null 2>&1; then
    missing+=("$dependency")
  fi
done
if (( ${#missing} )); then
  "$brew_bin" install "${missing[@]}"
fi

brew_prefix=$("$brew_bin" --prefix)
readonly brew_prefix
step "Настройка Release-сборки: $build_dir"
# Re-discover MD4C if Homebrew has replaced its versioned Cellar directory.
cmake -U '*MD4C*' -S "$project_root" -B "$build_dir" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$brew_prefix" \
  -DBUILD_TESTING=OFF -DMDREAD_TEST_NATIVE_WINDOW=OFF

jobs=$(sysctl -n hw.logicalcpu 2>/dev/null) || jobs=2
[[ "$jobs" == <1-> ]] || jobs=2
step "Сборка mdread ($jobs потоков)"
cmake --build "$build_dir" --config Release --target mdread --parallel "$jobs"

if $install_binary; then
  step "Установка mdread в $install_prefix/bin"
  cmake --install "$build_dir" --config Release --prefix "$install_prefix"
  print -r -- "Готово. Запуск: ${(q)install_prefix}/bin/mdread ."
  if (( ! ${path[(Ie)$install_prefix/bin]} )); then
    print -r -- "Для команды mdread добавьте в ~/.zshrc и выполните в текущем терминале:"
    print -r -- "export PATH=${(q)install_prefix}/bin:\$PATH"
  fi
else
  print -r -- "Готово. Запуск: ${(q)build_dir}/mdread ."
fi
