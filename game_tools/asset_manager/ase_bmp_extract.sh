#!/bin/bash

ASSET_SOURCE_DIR="./assets"
ASSET_BITMAP_OUTPUT_DIR="./assets/bitmaps"
ASSET_DATA_FILE_OUTPUT_DIR="./assets/aseprite_data"

mkdir -p $ASSET_SOURCE_DIR
mkdir -p $ASSET_BITMAP_OUTPUT_DIR
mkdir -p $ASSET_DATA_FILE_OUTPUT_DIR

for filepath in $ASSET_SOURCE_DIR/*_anim.aseprite; do
	filename_with_ext="${filepath##*/}"
	filename_no_meta="${filename_with_ext%_anim.*}"
	filename_no_ext="${filename_with_ext%.*}"
	filename_no_ext_lowercase=$(echo "$filename_no_ext" | tr '[:upper:]' '[:lower:]')
	filename_no_meta_lowercase=$(echo "$filename_no_meta" | tr '[:upper:]' '[:lower:]')

	echo "processing anim: $filename_with_ext..."

	aseprite -b $filepath \
	 	--data "$ASSET_DATA_FILE_OUTPUT_DIR/${filename_no_ext_lowercase}.json" \
		--list-tags \
		--save-as "$ASSET_BITMAP_OUTPUT_DIR/${filename_no_meta_lowercase}__{tag}__{tagframe}.png"
done

for filepath in $ASSET_SOURCE_DIR/*_sheet.aseprite; do
	filename="${filepath##*/}"
	filename_no_ext="${filename%.*}"
	filename_no_ext_lowercase=$(echo "$filename_no_ext" | tr '[:upper:]' '[:lower:]')

	echo "processing sheet: $filename..."

	aseprite -b $filepath \
		--split-slices --list-slices \
		--save-as "$ASSET_BITMAP_OUTPUT_DIR/{slice}.png"
done

