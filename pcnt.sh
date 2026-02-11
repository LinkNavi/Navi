#!/bin/bash
# pcnt_migration.sh - Automated PCNT migration script

set -e

FIRMWARE_DIR="Firmware/main"

echo "========================================="
echo "PCNT Rotary Encoder Migration Script"
echo "========================================="
echo ""

# Backup first
echo "[1/7] Creating backups..."
mkdir -p backups
cp -r ${FIRMWARE_DIR}/*.c backups/ 2>/dev/null || true
cp -r ${FIRMWARE_DIR}/include/*.h backups/ 2>/dev/null || true
cp ${FIRMWARE_DIR}/CMakeLists.txt backups/ 2>/dev/null || true
echo "✓ Backups created in backups/"
echo ""

# Update CMakeLists.txt
echo "[2/7] Updating CMakeLists.txt..."
if grep -q "esp_driver_pcnt" ${FIRMWARE_DIR}/CMakeLists.txt; then
    echo "⚠ esp_driver_pcnt already in CMakeLists.txt, skipping"
else
    sed -i 's/REQUIRES fatfs driver esp_driver_rmt nvs_flash esp_wifi esp_netif esp_event/REQUIRES fatfs driver esp_driver_rmt nvs_flash esp_wifi esp_netif esp_event\n             esp_driver_pcnt/' ${FIRMWARE_DIR}/CMakeLists.txt
    echo "✓ Added esp_driver_pcnt to CMakeLists.txt"
fi
echo ""

# Copy new header
echo "[3/7] Installing rotary_pcnt.h..."
if [ ! -f "rotary_pcnt.h" ]; then
    echo "✗ Error: rotary_pcnt.h not found in current directory"
    echo "  Please place rotary_pcnt.h in the same directory as this script"
    exit 1
fi
cp rotary_pcnt.h ${FIRMWARE_DIR}/include/drivers/
echo "✓ Installed rotary_pcnt.h"
echo ""

# Backup old rotary.h
echo "[4/7] Backing up old rotary.h..."
if [ -f "${FIRMWARE_DIR}/include/drivers/rotary.h" ]; then
    mv ${FIRMWARE_DIR}/include/drivers/rotary.h \
       ${FIRMWARE_DIR}/include/drivers/rotary_gpio.h
    echo "✓ Renamed rotary.h → rotary_gpio.h (backup)"
else
    echo "⚠ rotary.h not found, skipping backup"
fi
echo ""

# Update includes
echo "[5/7] Updating includes..."
FILES=(
    "${FIRMWARE_DIR}/Main.c"
    "${FIRMWARE_DIR}/wifi_menu.c"
    "${FIRMWARE_DIR}/xbegone_menu.c"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        sed -i 's/#include "drivers\/rotary.h"/#include "drivers\/rotary_pcnt.h"/' "$file"
        echo "  ✓ Updated $file"
    fi
done
echo ""

# Update types
echo "[6/7] Updating types and function calls..."
FILES=(
    "${FIRMWARE_DIR}/Main.c"
    "${FIRMWARE_DIR}/wifi_menu.c"
    "${FIRMWARE_DIR}/xbegone_menu.c"
    "${FIRMWARE_DIR}/include/rotary_text_input.h"
    "${FIRMWARE_DIR}/include/rotary_debug.h"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        # Update type declarations
        sed -i 's/Rotary encoder/RotaryPCNT encoder/g' "$file"
        sed -i 's/(Rotary \*encoder)/(RotaryPCNT *encoder)/g' "$file"
        
        # Update function calls
        sed -i 's/rotary_init(/rotary_pcnt_init(/g' "$file"
        sed -i 's/rotary_read(/rotary_pcnt_read(/g' "$file"
        sed -i 's/rotary_button_pressed(/rotary_pcnt_button_pressed(/g' "$file"
        sed -i 's/rotary_get_position(/rotary_pcnt_get_position(/g' "$file"
        sed -i 's/rotary_reset_position(/rotary_pcnt_reset_position(/g' "$file"
        
        echo "  ✓ Updated $file"
    fi
done
echo ""

# Summary
echo "[7/7] Migration complete!"
echo ""
echo "========================================="
echo "Summary of Changes"
echo "========================================="
echo "✓ CMakeLists.txt - Added esp_driver_pcnt"
echo "✓ rotary_pcnt.h - Installed to include/drivers/"
echo "✓ rotary.h - Renamed to rotary_gpio.h (backup)"
echo "✓ Main.c - Updated types and calls"
echo "✓ wifi_menu.c - Updated types and calls"
echo "✓ xbegone_menu.c - Updated types and calls"
echo "✓ rotary_text_input.h - Updated signatures"
echo "✓ rotary_debug.h - Updated signatures (if exists)"
echo ""
echo "Next steps:"
echo "1. Review changes: git diff"
echo "2. Build: cd Firmware && idf.py build"
echo "3. Flash: idf.py flash monitor"
echo ""
echo "To rollback: cp -r backups/* ${FIRMWARE_DIR}/"
echo ""
echo "========================================="
