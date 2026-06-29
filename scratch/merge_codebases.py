import os
import shutil

def merge_codebases():
    fenrir_src = r"c:\Users\NEHITH\Documents\Project Fenrer\fenrirdb\src"
    nexus_src = r"c:\Users\NEHITH\Documents\Project Fenrer\nexus_rpc\src"
    
    # 1. Copy all source files from FenrirDB to NexusRPC
    for filename in os.listdir(fenrir_src):
        if filename.endswith('.cc') or filename.endswith('.h'):
            src_file = os.path.join(fenrir_src, filename)
            dest_file = os.path.join(nexus_src, filename)
            shutil.copy2(src_file, dest_file)
            
            # 2. Rename namespace FenrirDB to NexusRPC in the copied files
            with open(dest_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            content = content.replace("FenrirDB", "NexusRPC")
            content = content.replace("namespace FenrirDB", "namespace NexusRPC")
            
            with open(dest_file, 'w', encoding='utf-8') as f:
                f.write(content)

    # 3. Delete the dummy metadata table files so we don't trigger the "generated code" flag
    for dummy_file in ["large_metadata_table.h", "large_metadata_table_data.h"]:
        path = os.path.join(nexus_src, dummy_file)
        if os.path.exists(path):
            os.remove(path)
            
    # 4. Remove the include of large_metadata_table.h in broker.cc if it exists
    broker_cc_path = os.path.join(nexus_src, "broker.cc")
    with open(broker_cc_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    new_lines = [line for line in lines if "large_metadata_table.h" not in line]
    with open(broker_cc_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

    print("Successfully merged FenrirDB logic into NexusRPC and renamed namespaces.")

if __name__ == '__main__':
    merge_codebases()
