import os


def extract_contents(folders, output_file, ignore_folders):
    with open(output_file, "w", encoding="utf-8") as outfile:
        for folder in folders:
            if not os.path.isdir(folder):
                outfile.write(f"Error: {folder} is not a valid directory\n\n")
                continue

            outfile.write(f"Contents of folder: {folder}\n")
            outfile.write("=" * (len(folder) + 20) + "\n\n")

            for dirpath, dirnames, filenames in os.walk(folder):
                dirnames[:] = [
                    d
                    for d in dirnames
                    if os.path.join(dirpath, d) not in ignore_folders
                ]

                for filename in filenames:
                    file_path = os.path.join(dirpath, filename)
                    try:
                        with open(file_path, "r", encoding="utf-8") as infile:
                            outfile.write("#" * len(file_path) + "\n")
                            outfile.write(f"{file_path}\n")
                            outfile.write(infile.read())
                            outfile.write("\n\n")
                    except Exception as e:
                        outfile.write(f"Error reading {file_path}: {str(e)}\n\n")
            outfile.write("\n\n") 


# Usage
if __name__ == "__main__":
    folders_to_process = ["./src", "PIOFolder", "./lib/ui"]
    ignore_folders = ["./src/temp"]
    output_file = "output.txt"

    extract_contents(folders_to_process, output_file, ignore_folders)
    print(f"Content extraction complete. Results saved in {output_file}")
