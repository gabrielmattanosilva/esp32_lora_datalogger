import os

def concatenate_c_h_files_in_lib_src(base_dir="."):
    target_dirs = ["include", "lib", "src"]
    concatenated_files = []

    for folder in target_dirs:
        full_path = os.path.join(base_dir, folder)
        if not os.path.isdir(full_path):
            continue  # skip if the folder does not exist

        for root, dirs, files in os.walk(full_path):
            for filename in sorted(files):
                if filename.endswith(".c") or filename.endswith(".cpp") or filename.endswith(".h"):
                    file_path = os.path.join(root, filename)
                    with open(file_path, "r", encoding="utf-8") as f:
                        content = f.read()
                        header = f"\n/********** {os.path.relpath(file_path, base_dir)} **********/\n\n"
                        concatenated_files.append(header + content)

    # Set output filename to the base directory name
    base_name = os.path.basename(os.path.abspath(base_dir.rstrip("/\\")))
    output_file = f"{base_name}.txt"

    with open(output_file, "w", encoding="utf-8") as out_file:
        out_file.write("\n".join(concatenated_files))

    print(f"Concatenated files saved to: {output_file}")

def main():
    base_directory = "."  # Change this to your target root folder if needed
    concatenate_c_h_files_in_lib_src(base_directory)

if __name__ == "__main__":
    main()
