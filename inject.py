import sys
import psutil
from pyinjector import inject

if len(sys.argv) != 3:
    print("Usage: python inject.py <process-name> <shared-library>")
    exit()

_, process_name, shared_library = sys.argv

for process in psutil.process_iter():
    if process.name() == process_name:
        print(f"Found {process_name} - injecting {shared_library} into PID {process.pid}")
        inject(process.pid, shared_library)
        print("Injected successfully")
        exit()

print(f"Unable to find process named {process_name}")