import os
import re

def cleanup_codebase():
    nexus_src = r"c:\Users\NEHITH\Documents\Project Fenrer\nexus_rpc\src"
    
    # Keywords indicating planted/injected bugs
    bug_keywords = [
        r"//\s*INJECTED\s*BUG.*",
        r"//\s*DELIBERATE\s*BUG.*",
        r"//\s*Trigger\s*point\s*for\s*Bug.*",
        r"//\s*Can\s*return\s*a\s*dangling.*",
        r"//\s*Will\s*trigger\s*Double\s*Free.*",
        r"//\s*Accesses\s*out\s*of\s*bounds.*",
        r"//\s*If\s*the\s*fuzzer\s*specifies.*",
        r"//\s*Out-of-bounds\s*stack\s*read.*",
        r"//\s*Double-free\s*risk.*",
        r"//\s*Instead\s*of\s*using\s*modulo.*",
        r"//\s*The\s*index\s*calculation\s*lacks.*",
        r"//\s*We\s*delete\s*the\s*old\s*session.*",
        r"//\s*This\s*leaves\s*the\s*key.*",
        r"//\s*We\s*forget\s*to\s*erase.*",
        r"//\s*If\s*client\s*is\s*already\s*connected.*",
        r"//\s*Verify\s*session.*",
        r"//\s*Trigger\s*Type\s*Confusion.*"
    ]
    
    compiled_keywords = [re.compile(pattern, re.IGNORECASE) for pattern in bug_keywords]

    for filename in os.listdir(nexus_src):
        if filename.endswith('.cc') or filename.endswith('.h'):
            file_path = os.path.join(nexus_src, filename)
            
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # 1. Fix header guards (convert FENRIRDB_ to NEXUS_RPC_)
            content = re.sub(r'FENRIRDB_', 'NEXUS_RPC_', content)
            
            # 2. Strip out specific comments about injected/deliberate bugs
            lines = content.split('\n')
            new_lines = []
            for line in lines:
                matched = False
                for pattern in compiled_keywords:
                    if pattern.search(line):
                        matched = True
                        break
                if not matched:
                    new_lines.append(line)
            
            content = '\n'.join(new_lines)
            
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)

    print("Successfully cleaned up header guards and removed synthetic bug comments.")

if __name__ == '__main__':
    cleanup_codebase()
