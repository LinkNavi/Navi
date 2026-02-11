#!/usr/bin/env python3
"""
IR to C Header Converter - Fixed version
Handles duplicate names and invalid C identifiers
"""

import os
import re
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple

class IRSignal:
    def __init__(self, name: str, protocol: str, address: str, command: str):
        self.name = name
        self.protocol = protocol
        self.address = address
        self.command = command
    
    def to_c_struct(self) -> str:
        return f'    {{"{self.name}", "{self.protocol}", "{self.address}", "{self.command}"}}'

class IRDevice:
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
        name = f"{safe_brand}_{safe_model}".upper()
        # C identifiers can't start with a digit
        if name[0].isdigit():
            name = f"DEV_{name}"
        return name

def parse_ir_file(filepath: Path) -> Tuple[str, List[IRSignal]]:
    signals = []
    current_signal = {}
    device_name = filepath.stem
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            if not line or line.startswith('Filetype:') or line.startswith('Version:'):
                continue
            
            if line.startswith('#'):
                continue
            
            if line.startswith('name:'):
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
    categories = defaultdict(list)
    
    for category_dir in sorted(ir_path.iterdir()):
        if not category_dir.is_dir():
            continue
        
        if category_dir.name.startswith('_') or category_dir.name.startswith('.'):
            continue
        
        category_name = category_dir.name
        
        for brand_dir in sorted(category_dir.iterdir()):
            if not brand_dir.is_dir():
                if brand_dir.suffix == '.ir':
                    device_name, signals = parse_ir_file(brand_dir)
                    device = IRDevice("Generic", device_name, category_name)
                    for signal in signals:
                        device.add_signal(signal)
                    if device.signals:
                        categories[category_name].append(device)
                continue
            
            brand_name = brand_dir.name
            
            for ir_file in sorted(brand_dir.glob('*.ir')):
                device_name, signals = parse_ir_file(ir_file)
                device = IRDevice(brand_name, device_name, category_name)
                for signal in signals:
                    device.add_signal(signal)
                
                if device.signals:
                    categories[category_name].append(device)
    
    return categories

def generate_header_file(categories: Dict[str, List[IRDevice]], output_path: Path):
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("""/**
 * IR Signal Database
 * Auto-generated from IR signal files
 */

#ifndef IR_SIGNALS_H
#define IR_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct {
    const char* name;
    const char* protocol;
    const char* address;
    const char* command;
} IRSignal;

typedef struct {
    const char* brand;
    const char* model;
    const char* category;
    const IRSignal* signals;
    size_t signal_count;
} IRDevice;

""")
        
        f.write("/* ========== IR Signal Data ========== */\n\n")
        
        all_devices = []
        seen_names = {}  # Track duplicate names
        
        for category_name in sorted(categories.keys()):
            f.write(f"/* Category: {category_name} */\n\n")
            
            for device in categories[category_name]:
                safe_name = device.get_safe_name()
                
                # Handle duplicates by appending counter
                if safe_name in seen_names:
                    seen_names[safe_name] += 1
                    safe_name = f"{safe_name}_{seen_names[safe_name]}"
                else:
                    seen_names[safe_name] = 0
                
                f.write(f"/* {device.brand} {device.model} */\n")
                f.write(f"static const IRSignal {safe_name}_SIGNALS[] = {{\n")
                
                for signal in device.signals:
                    f.write(signal.to_c_struct())
                    f.write(",\n")
                
                f.write("};\n\n")
                
                all_devices.append((device, safe_name))
        
        f.write("/* ========== Device Index ========== */\n\n")
        f.write("static const IRDevice IR_DEVICES[] = {\n")
        
        for device, safe_name in all_devices:
            f.write(f'    {{"{device.brand}", "{device.model}", "{device.category}", '
                   f'{safe_name}_SIGNALS, sizeof({safe_name}_SIGNALS) / sizeof(IRSignal)}},\n')
        
        f.write("};\n\n")
        
        f.write(f"#define IR_DEVICE_COUNT {len(all_devices)}\n\n")
        
        f.write("/* Helper functions */\n")
        f.write("""
static inline const IRDevice* ir_get_device(size_t index) {
    if (index >= IR_DEVICE_COUNT) return NULL;
    return &IR_DEVICES[index];
}

#ifdef __cplusplus
}
#endif

#endif /* IR_SIGNALS_H */
""")

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Convert IR signal files to C header format')
    parser.add_argument('--input', '-i', type=Path, default=Path('IR'),
                       help='Input directory containing IR files (default: IR)')
    parser.add_argument('--output', '-o', type=Path, default=Path('ir_signals.h'),
                       help='Output header file (default: ir_signals.h)')
    parser.add_argument('--stats', '-s', action='store_true',
                       help='Print statistics about the IR database')
    
    args = parser.parse_args()
    
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
        total_devices = sum(len(devices) for devices in categories.values())
        total_signals = sum(
            sum(len(device.signals) for device in devices)
            for devices in categories.values()
        )
        
        print("\n" + "="*50)
        print("Database Statistics")
        print("="*50)
        print(f"Total Categories: {len(categories)}")
        print(f"Total Devices: {total_devices}")
        print(f"Total Signals: {total_signals}")
        print("\nDevices per category:")
        
        for category in sorted(categories.keys()):
            device_count = len(categories[category])
            signal_count = sum(len(d.signals) for d in categories[category])
            print(f"  {category}: {device_count} devices, {signal_count} signals")
    
    return 0

if __name__ == '__main__':
    exit(main())
