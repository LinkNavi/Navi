
#!/usr/bin/env python3
"""
IR to C Header Converter
Parses Flipper Zero IR signal files and generates organized C header files
"""

import os
import re
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple

class IRSignal:
    """Represents a single IR signal"""
    def __init__(self, name: str, protocol: str, address: str, command: str):
        self.name = name
        self.protocol = protocol
        self.address = address
        self.command = command
    
    def to_c_struct(self, var_prefix: str) -> str:
        """Convert to C struct initialization"""
        # Clean up the name for use as a C identifier
        c_name = re.sub(r'[^a-zA-Z0-9_]', '_', self.name)
        return f'    {{"{self.name}", "{self.protocol}", "{self.address}", "{self.command}"}}'

class IRDevice:
    """Represents a device with multiple IR signals"""
    def __init__(self, brand: str, model: str, category: str):
        self.brand = brand
        self.model = model
        self.category = category
        self.signals: List[IRSignal] = []
    
    def add_signal(self, signal: IRSignal):
        self.signals.append(signal)
    
    def get_safe_name(self) -> str:
        """Get a safe C identifier for this device"""
        safe_brand = re.sub(r'[^a-zA-Z0-9_]', '_', self.brand)
        safe_model = re.sub(r'[^a-zA-Z0-9_]', '_', self.model)
        return f"{safe_brand}_{safe_model}".upper()

def parse_ir_file(filepath: Path) -> Tuple[str, List[IRSignal]]:
    """Parse a single IR file and return device name and signals"""
    signals = []
    current_signal = {}
    device_name = filepath.stem
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Skip empty lines and file headers
            if not line or line.startswith('Filetype:') or line.startswith('Version:'):
                continue
            
            # Skip comments that aren't signal data
            if line.startswith('#'):
                continue
            
            # Parse signal properties
            if line.startswith('name:'):
                # Save previous signal if exists
                if current_signal and 'name' in current_signal:
                    if current_signal.get('type') == 'parsed':
                        signals.append(IRSignal(
                            current_signal['name'],
                            current_signal.get('protocol', 'Unknown'),
                            current_signal.get('address', '00 00 00 00'),
                            current_signal.get('command', '00 00 00 00')
                        ))
                current_signal = {}
                current_signal['name'] = line.split(':', 1)[1].strip()
            
            elif line.startswith('type:'):
                current_signal['type'] = line.split(':', 1)[1].strip()
            
            elif line.startswith('protocol:'):
                current_signal['protocol'] = line.split(':', 1)[1].strip()
            
            elif line.startswith('address:'):
                current_signal['address'] = line.split(':', 1)[1].strip()
            
            elif line.startswith('command:'):
                current_signal['command'] = line.split(':', 1)[1].strip()
        
        # Don't forget the last signal
        if current_signal and 'name' in current_signal:
            if current_signal.get('type') == 'parsed':
                signals.append(IRSignal(
                    current_signal['name'],
                    current_signal.get('protocol', 'Unknown'),
                    current_signal.get('address', '00 00 00 00'),
                    current_signal.get('command', '00 00 00 00')
                ))
    
    return device_name, signals

def scan_ir_directory(ir_path: Path) -> Dict[str, List[IRDevice]]:
    """Scan IR directory and organize devices by category"""
    categories = defaultdict(list)
    
    # Iterate through category folders
    for category_dir in sorted(ir_path.iterdir()):
        if not category_dir.is_dir():
            continue
        
        # Skip hidden or special directories
        if category_dir.name.startswith('_') or category_dir.name.startswith('.'):
            continue
        
        category_name = category_dir.name
        
        # Iterate through brand folders
        for brand_dir in sorted(category_dir.iterdir()):
            if not brand_dir.is_dir():
                # Handle IR files directly in category folder
                if brand_dir.suffix == '.ir':
                    device_name, signals = parse_ir_file(brand_dir)
                    device = IRDevice("Generic", device_name, category_name)
                    for signal in signals:
                        device.add_signal(signal)
                    if device.signals:
                        categories[category_name].append(device)
                continue
            
            brand_name = brand_dir.name
            
            # Iterate through IR files in brand folder
            for ir_file in sorted(brand_dir.glob('*.ir')):
                device_name, signals = parse_ir_file(ir_file)
                device = IRDevice(brand_name, device_name, category_name)
                for signal in signals:
                    device.add_signal(signal)
                
                if device.signals:
                    categories[category_name].append(device)
    
    return categories

def generate_header_file(categories: Dict[str, List[IRDevice]], output_path: Path):
    """Generate the main C header file with all IR data"""
    
    with open(output_path, 'w', encoding='utf-8') as f:
        # Write header guard and includes
        f.write("""/**
 * IR Signal Database
 * Auto-generated from IR signal files
 * 
 * This header contains organized IR signal data for various devices
 */

#ifndef IR_SIGNALS_H
#define IR_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * IR Signal structure
 */
typedef struct {
    const char* name;       /* Signal name (e.g., "Power", "Vol_up") */
    const char* protocol;   /* Protocol type (e.g., "NECext", "Samsung32") */
    const char* address;    /* Address bytes */
    const char* command;    /* Command bytes */
} IRSignal;

/**
 * IR Device structure
 */
typedef struct {
    const char* brand;      /* Device brand */
    const char* model;      /* Device model */
    const char* category;   /* Device category */
    const IRSignal* signals;/* Array of signals */
    size_t signal_count;    /* Number of signals */
} IRDevice;

""")
        
        # Generate signal arrays for each device
        f.write("/* ========== IR Signal Data ========== */\n\n")
        
        all_devices = []
        
        for category_name in sorted(categories.keys()):
            f.write(f"/* Category: {category_name} */\n\n")
            
            for device in categories[category_name]:
                safe_name = device.get_safe_name()
                
                # Write signal array
                f.write(f"/* {device.brand} {device.model} */\n")
                f.write(f"static const IRSignal {safe_name}_SIGNALS[] = {{\n")
                
                for signal in device.signals:
                    f.write(signal.to_c_struct(safe_name))
                    f.write(",\n")
                
                f.write("};\n\n")
                
                all_devices.append((device, safe_name))
        
        # Generate device array
        f.write("/* ========== Device Index ========== */\n\n")
        f.write("static const IRDevice IR_DEVICES[] = {\n")
        
        for device, safe_name in all_devices:
            f.write(f'    {{"{device.brand}", "{device.model}", "{device.category}", '
                   f'{safe_name}_SIGNALS, sizeof({safe_name}_SIGNALS) / sizeof(IRSignal)}},\n')
        
        f.write("};\n\n")
        
        # Generate constants
        f.write(f"#define IR_DEVICE_COUNT {len(all_devices)}\n\n")
        
        # Generate category enum
        f.write("/* Category enum for easy filtering */\n")
        f.write("typedef enum {\n")
        for i, category in enumerate(sorted(categories.keys())):
            enum_name = re.sub(r'[^a-zA-Z0-9_]', '_', category).upper()
            f.write(f"    IR_CATEGORY_{enum_name} = {i},\n")
        f.write(f"    IR_CATEGORY_COUNT = {len(categories)}\n")
        f.write("} IRCategory;\n\n")
        
        # Generate category name lookup
        f.write("/* Category name lookup */\n")
        f.write("static const char* IR_CATEGORY_NAMES[] = {\n")
        for category in sorted(categories.keys()):
            f.write(f'    "{category}",\n')
        f.write("};\n\n")
        
        # Generate helper functions declarations
        f.write("""/* ========== Helper Functions ========== */

/**
 * Get device by index
 * @param index Device index (0 to IR_DEVICE_COUNT-1)
 * @return Pointer to device or NULL if index is invalid
 */
static inline const IRDevice* ir_get_device(size_t index) {
    if (index >= IR_DEVICE_COUNT) return NULL;
    return &IR_DEVICES[index];
}

/**
 * Find devices by category
 * @param category Category name to search for
 * @param results Array to store matching device pointers
 * @param max_results Maximum number of results to return
 * @return Number of devices found
 */
static inline size_t ir_find_by_category(const char* category, 
                                          const IRDevice** results, 
                                          size_t max_results) {
    size_t count = 0;
    for (size_t i = 0; i < IR_DEVICE_COUNT && count < max_results; i++) {
        if (strcmp(IR_DEVICES[i].category, category) == 0) {
            results[count++] = &IR_DEVICES[i];
        }
    }
    return count;
}

/**
 * Find devices by brand
 * @param brand Brand name to search for
 * @param results Array to store matching device pointers
 * @param max_results Maximum number of results to return
 * @return Number of devices found
 */
static inline size_t ir_find_by_brand(const char* brand, 
                                       const IRDevice** results, 
                                       size_t max_results) {
    size_t count = 0;
    for (size_t i = 0; i < IR_DEVICE_COUNT && count < max_results; i++) {
        if (strcmp(IR_DEVICES[i].brand, brand) == 0) {
            results[count++] = &IR_DEVICES[i];
        }
    }
    return count;
}

/**
 * Find a specific signal in a device by name
 * @param device Pointer to device
 * @param signal_name Name of the signal to find
 * @return Pointer to signal or NULL if not found
 */
static inline const IRSignal* ir_find_signal(const IRDevice* device, 
                                              const char* signal_name) {
    if (!device) return NULL;
    for (size_t i = 0; i < device->signal_count; i++) {
        if (strcmp(device->signals[i].name, signal_name) == 0) {
            return &device->signals[i];
        }
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* IR_SIGNALS_H */
""")

def generate_stats(categories: Dict[str, List[IRDevice]]) -> str:
    """Generate statistics about the IR database"""
    total_devices = sum(len(devices) for devices in categories.values())
    total_signals = sum(
        sum(len(device.signals) for device in devices)
        for devices in categories.values()
    )
    
    stats = []
    stats.append(f"Total Categories: {len(categories)}")
    stats.append(f"Total Devices: {total_devices}")
    stats.append(f"Total Signals: {total_signals}")
    stats.append("\nDevices per category:")
    
    for category in sorted(categories.keys()):
        device_count = len(categories[category])
        signal_count = sum(len(d.signals) for d in categories[category])
        stats.append(f"  {category}: {device_count} devices, {signal_count} signals")
    
    return "\n".join(stats)

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Convert IR signal files to C header format'
    )
    parser.add_argument(
        '--input', '-i',
        type=Path,
        default=Path('IR'),
        help='Input directory containing IR files (default: IR)'
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=Path('ir_signals.h'),
        help='Output header file (default: ir_signals.h)'
    )
    parser.add_argument(
        '--stats', '-s',
        action='store_true',
        help='Print statistics about the IR database'
    )
    
    args = parser.parse_args()
    
    # Check if input directory exists
    if not args.input.exists():
        print(f"Error: Input directory '{args.input}' does not exist")
        return 1
    
    print(f"Scanning IR files in: {args.input}")
    categories = scan_ir_directory(args.input)
    
    if not categories:
        print("No IR files found!")
        return 1
    
    print(f"Generating header file: {args.output}")
    generate_header_file(categories, args.output)
    
    print(f"Successfully generated {args.output}")
    
    if args.stats:
        print("\n" + "="*50)
        print("Database Statistics")
        print("="*50)
        print(generate_stats(categories))
    
    return 0

if __name__ == '__main__':
    exit(main())
