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
                # Remove ignored folders from dirnames to prevent os.walk from traversing them
                dirnames[:] = [
                    d
                    for d in dirnames
                    if os.path.join(dirpath, d) not in ignore_folders
                ]

                for filename in filenames:
                    file_path = os.path.join(dirpath, filename)
                    try:
                        with open(file_path, "r", encoding="utf-8") as infile:
                            # Write the file path followed by a line of "#" characters
                            outfile.write("#" * len(file_path) + "\n")
                            outfile.write(f"{file_path}\n")

                            # Write the contents of the file
                            outfile.write(infile.read())

                            # Add two newlines for separation
                            outfile.write("\n\n")
                    except Exception as e:
                        outfile.write(f"Error reading {file_path}: {str(e)}\n\n")

            outfile.write("\n\n")  # Add extra separation between folders


# Usage
if __name__ == "__main__":
    # Hard-coded list of folders to process
    # folders_to_process = ["./src", "PIOFolder", "./lib/ui"]
    folders_to_process = ["./src"]

    # List of folders to ignore (use absolute paths or paths relative to the script's location)
    ignore_folders = ["./src/temp"]

    # Output file name
    output_file = "output.txt"

    extract_contents(folders_to_process, output_file, ignore_folders)
    print(f"Content extraction complete. Results saved in {output_file}")
