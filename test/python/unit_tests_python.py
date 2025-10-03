import subprocess
import sys

# List of script files to run
scripts = [
    "dtl_config.py",
    "dtl_connection.py",
    "dtl_file_engine.py",
    "dtl_staging_engine.py",
    "dtl_stream.py",
    "dtl_variable.py"
    ]
                        
def run_script(script):
    print(f"\nRunning {script}...")
    try:
        result = subprocess.run([sys.executable, script], check=True, capture_output=True, text=True)
        print(f"✅ {script} completed successfully.")
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ {script} failed with error:")
        print(e.stderr)
        return False
                                                                                
if __name__ == "__main__":
    all_passed = True
    for script in scripts:
        if not run_script(script):
            all_passed = False
    if not all_passed:
        sys.exit(1)
