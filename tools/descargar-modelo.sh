#!/bin/sh
# Descarga el modelo de detección de cara que usa `ctl track`.
# YuNet (opencv_zoo, licencia Apache-2.0): 232.589 bytes.
# El repositorio de OpenCV sirve el binario desde Hugging Face; la ruta de
# raw.githubusercontent.com da 404, así que se usa esa y se comprueba la suma.
set -e
DIR="$(dirname "$0")/.."
DEST="$DIR/modelos/face_detection_yunet_2023mar.onnx"
URL="https://huggingface.co/opencv/face_detection_yunet/resolve/main/face_detection_yunet_2023mar.onnx"
SHA="8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4"

if [ -f "$DEST" ]; then
  echo "ya está: $DEST ($(wc -c <"$DEST") bytes)"
  exit 0
fi
mkdir -p "$DIR/modelos"
echo "descargando $URL"
curl -L --fail --max-time 120 -o "$DEST.part" "$URL"
GOT=$(sha256sum "$DEST.part" | cut -d" " -f1)
if [ "$GOT" != "$SHA" ]; then
  echo "suma distinta: $GOT (se esperaba $SHA)" >&2
  head -c 200 "$DEST.part" >&2
  rm -f "$DEST.part"
  exit 1
fi
mv "$DEST.part" "$DEST"
echo "listo: $DEST"
