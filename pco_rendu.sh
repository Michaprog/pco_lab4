#!/usr/bin/env bash
set -euo pipefail

SCRIPT="${BASH_SOURCE[0]}"
SCRIPTPATH="$(realpath "$(dirname "$SCRIPT")")"

PROJECT_DIR="code"
SRC_DIR="$PROJECT_DIR/src"
TEST_DIR="$PROJECT_DIR/tests"
ARCHIVE="rendu.tar.gz"
REPORT_FILE="rapport.pdf"

cd "$SCRIPTPATH"

RED="\033[1;31m"
NC="\033[0m" # No Color

if [[ ! -d "$SRC_DIR" ]]; then
  echo -e "${RED}Dossier source introuvable : $SRC_DIR${NC}" >&2
  exit 1
fi

if [[ ! -d "$TEST_DIR" ]]; then
  echo -e "${RED}Dossier tests introuvable : $TEST_DIR${NC}" >&2
  exit 1
fi

if [[ ! -f "$REPORT_FILE" ]]; then
  echo -e "${RED}Rapport manquant : $REPORT_FILE${NC}" >&2
  echo -e "${RED}   Merci de placer le fichier '$REPORT_FILE' à la racine du projet.${NC}" >&2
  exit 1
fi

FILELIST="$(mktemp)"
trap 'rm -f "$FILELIST"' EXIT

find "$SRC_DIR"  -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) | sort >>"$FILELIST"
find "$TEST_DIR" -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) | sort >>"$FILELIST"

# [[ -f "CMakeLists.txt" ]] && echo "CMakeLists.txt" >>"$FILELIST"
[[ -f "$PROJECT_DIR/CMakeLists.txt" ]] && echo "$PROJECT_DIR/CMakeLists.txt" >>"$FILELIST"

echo "$REPORT_FILE" >>"$FILELIST"

echo "Les fichiers suivants seront inclus dans $ARCHIVE :"
cat "$FILELIST"
echo

tar --exclude="$ARCHIVE" -czvf "$ARCHIVE" -T "$FILELIST"

echo
echo "Archive créée : $SCRIPTPATH/$ARCHIVE"
