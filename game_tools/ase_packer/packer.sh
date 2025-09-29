#!/bin/bash

ASSET_SOURCE_DIR="./assets"
ASSET_OUTPUT_DIR="./assets/bitmaps"
OUTPUT_SPRITE_SHEET="$ASSET_OUTPUT_DIR/packed_sprite.png"
OUTPUT_DATA_FILE="$ASSET_OUTPUT_DIR/packed_sprite_data.json"

ASEPRITE_FILES="./assets/Hero.aseprite \
./assets/Warrior.aseprite "

ASEPRITE_AND_PNG_FILES=$(find $ASSET_SOURCE_DIR -maxdepth 1 -type f \( -name "*.aseprite" -o -name "*.png" \) | tr '\n' ' ')

for filepath in $ASSET_SOURCE_DIR/*.aseprite; do
	filename_with_ext="${filepath##*/}"
	filename="${filename_with_ext%.*}"
	filename_lowercase=$(echo "$filename" | tr '[:upper:]' '[:lower:]')
	aseprite -b $filepath \
	 	--data "$ASSET_OUTPUT_DIR/${filename_lowercase}.json" \
		--list-tags \
		--save-as "$ASSET_OUTPUT_DIR/${filename_lowercase}__{tag}__{tagframe}.png"
done

