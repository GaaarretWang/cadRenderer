#!/bin/bash
# Test single frame rendering and check for black pixels

SCENE_ID=${1:-0}
OUTPUT_FILE="test_output_scene_${SCENE_ID}.png"

echo "Testing scene ${SCENE_ID}..."
timeout 10s ./Rendering -s ${SCENE_ID} > /dev/null 2>&1

if [ -f "$OUTPUT_FILE" ]; then
    # Check if image is mostly black (using ImageMagick if available)
    if command -v identify &> /dev/null; then
        # Get average pixel value
        AVG=$(convert "$OUTPUT_FILE" -colorspace gray -format "%[fx:mean*100]" info:)
        echo "Average brightness: ${AVG}%"

        # Check if mostly black (< 5% brightness)
        if (( $(echo "$AVG < 5" | bc -l) )); then
            echo "WARNING: Image appears to be mostly black!"
            exit 1
        else
            echo "Image looks OK (brightness > 5%)"
            exit 0
        fi
    else
        echo "ImageMagick not available, checking file size..."
        SIZE=$(stat -c%s "$OUTPUT_FILE")
        echo "File size: $SIZE bytes"
        if [ $SIZE -lt 100000 ]; then
            echo "WARNING: File size suspiciously small!"
            exit 1
        fi
    fi
else
    echo "ERROR: Output file not found!"
    exit 1
fi
