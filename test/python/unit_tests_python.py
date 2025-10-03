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
    except subprocess.CalledProcessError as e:
        print(f"❌ {script} failed with error:")
        print(e.stderr)
                                                                                
if __name__ == "__main__":
    for script in scripts:
        run_script(script)
                                                                                                            