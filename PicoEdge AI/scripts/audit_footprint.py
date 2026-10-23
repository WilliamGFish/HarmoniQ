import os
import subprocess

def main():
    print("Footprint Audit Gate...")

    # We'll just generate the C file for memory size assertions
    c_source = """
#include <stdio.h>
#include "picoedge.h"

int main() {
    printf("RAM: %zu Bytes\\n", sizeof(struct pe_context));
    printf("Frame: %zu Bytes\\n", sizeof(pe_diag_frame_t));
    if (sizeof(struct pe_context) > 2048) return 1;
    if (sizeof(pe_diag_frame_t) != 12) return 1;
    return 0;
}
"""
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    c_path = "/tmp/audit.c"
    exe_path = "/tmp/audit.exe"

    with open(c_path, "w") as f:
        f.write(c_source)

    cmd = ["gcc", "-std=c99", f"-I{os.path.join(repo_root, 'include')}", c_path, "-o", exe_path]
    subprocess.check_call(cmd)

    result = subprocess.run([exe_path], capture_output=True, text=True)
    print(result.stdout)

    if result.returncode == 0:
        print("Memory footprint limits verified.")
    else:
        print("Memory limits exceeded!")
        exit(1)

if __name__ == "__main__":
    main()
