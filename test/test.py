import difflib
import filecmp
import os
import subprocess
import tempfile

# Quick and dirty test to check whether we have completely broken something.
# Does not check if the code has been recompiled if necessary.


def test(strategy_file, golden_file):
    with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        temp_file.close()
        print(f"The file is named {temp_file.name}")

        # Ugly that there is a full pathname here, but....
        prefix = "C:/Users/sigma/source/repos/video-poker/"
        subprocess.run(
            [
                prefix + "x64/Release/strategy.exe",
                f"{prefix}data/{strategy_file}",
                temp_file.name,
            ]
        )
        if filecmp.cmp(
            temp_file.name, f"{prefix}test/{golden_file}", shallow=False
        ):
            print("Test passes")
        else:
            print("Differences detected")
            with open(temp_file.name) as f1, open(
                f"{prefix}test/{golden_file}"
            ) as f2:
                for line in difflib.Differ().compare(
                    f1.readlines(), f2.readlines()
                ):
                    print(line, end="")
        os.remove(temp_file.name)


def run_tests():
    test("jacks-96.txt", "golden.txt")
    test("fpdw_practical.txt", "golden_deuces.txt")
    test("multi-jacks.txt", "multi-golden.txt")

if __name__ == "__main__":
    run_tests()
