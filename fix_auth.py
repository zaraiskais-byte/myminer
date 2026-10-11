import os

cpp_file = "src/caesar_web.cpp"
if os.path.exists(cpp_file):
    with open(cpp_file, "r") as f:
        content = f.read()
    print("caesar_web.cpp loaded successfully.")
else:
    print("caesar_web.cpp not found directly, checking subdirectories...")
