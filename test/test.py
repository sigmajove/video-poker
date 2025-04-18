import difflib
import filecmp
import os
import subprocess
import tempfile

# Quick and dirty test to check whether we have completely broken something.
# Does not check if the code has been recompiled if necessary.


def test():
    with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        temp_file.close()
        print(f"The file is named {temp_file.name}")

        # Ugly that there is a full pathname here, but....
        prefix = "C:/Users/sigma/source/repos/video-poker/"
        subprocess.run(
            [
                prefix + "x64/Release/strategy.exe",
                prefix + "data/jacks-96.txt",
                temp_file.name,
            ]
        )
        if filecmp.cmp(
            temp_file.name, prefix + "test/golden.txt", shallow=False
        ):
            print("Test passes")
        else:
            print("Differences detected")
            with open(temp_file.name) as f1, open(
                prefix + "test/golden.txt"
            ) as f2:
                for line in difflib.Differ().compare(
                    f1.readlines(), f2.readlines()
                ):
                    print(line, end="")
        os.remove(temp_file.name)


if __name__ == "__main__":
    test()
