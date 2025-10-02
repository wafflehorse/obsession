#!/bin/bash

ASSET_SOURCE_DIR="./assets"
ASSET_BITMAP_OUTPUT_DIR="./assets/bitmaps"
ASSET_DATA_FILE_OUTPUT_DIR="./assets/aseprite_data"

mkdir -p $ASSET_SOURCE_DIR
mkdir -p $ASSET_BITMAP_OUTPUT_DIR
mkdir -p $ASSET_DATA_FILE_OUTPUT_DIR

for filepath in $ASSET_SOURCE_DIR/*.aseprite; do
	filename_with_ext="${filepath##*/}"
	filename="${filename_with_ext%.*}"
	filename_lowercase=$(echo "$filename" | tr '[:upper:]' '[:lower:]')
	aseprite -b $filepath \
	 	--data "$ASSET_DATA_FILE_OUTPUT_DIR/${filename_lowercase}.json" \
		--list-tags \
		--save-as "$ASSET_BITMAP_OUTPUT_DIR/${filename_lowercase}__{tag}__{tagframe}.png"
done

