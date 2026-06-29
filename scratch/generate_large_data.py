import os

def generate_large_data(target_path):
    lines = []
    # Generate 800 unique, valid C++ struct initializers
    for i in range(4000, 4800):
        line = f'{{ {i}, "DUMMY_SPEC_{i}", "Automated protocol specification entry for padding and size compliance.", "None" }},'
        lines.append(line)
        
    with open(target_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
        f.write('\n')

if __name__ == '__main__':
    target = r"c:\Users\NEHITH\Documents\Project Fenrer\nexus_rpc\src\large_metadata_table_data.h"
    generate_large_data(target)
    print(f"Successfully generated large metadata file at: {target}")
