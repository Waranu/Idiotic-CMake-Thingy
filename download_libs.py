import argparse
import json
import git
import os
import shutil
import stat


def remove_readonly(func, path, exc_info):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def clone_repo(repo_url, branch, depth, output_path, unshallow, shallow_submodules):
    repo = git.Repo.clone_from(
        repo_url,
        output_path,
        branch=branch,
        depth=depth,
        recursive=True,
        shallow_submodules=shallow_submodules,
    )

    if unshallow:
        repo.git.fetch("--unshallow")

    print(f"Repository cloned to {output_path} on branch {branch} with depth {depth}.")
    if unshallow:
        print("Repository unshallowed.")


def main():
    parser = argparse.ArgumentParser(
        description="A script to clone specific Git repositories based on command-line flags."
    )

    parser.add_argument(
        "flags", nargs="*", help="Flags to specify which repositories to clone"
    )
    parser.add_argument(
        "--shallow_submodules",
        action="store_true",
        help="Perform a shallow clone of submodules",
    )

    args = parser.parse_args()

    libs_dir = "deps"

    if os.path.exists(libs_dir):
        shutil.rmtree(libs_dir, onerror=remove_readonly)

    os.makedirs(libs_dir, exist_ok=True)

    with open("repo_list.json", "r") as file:
        repo_list = json.load(file)

    for flag in args.flags:
        if flag in repo_list:
            repo = repo_list[flag]
            print(f"Cloning repository for flag: {flag}")
            clone_repo(
                repo["link"],
                repo["branch"],
                repo["depth"],
                os.path.join(
                    libs_dir,
                    os.path.basename(repo["link"]).replace(".git", ""),
                ),
                repo["unshallow"],
                args.shallow_submodules,
            )
        else:
            print(f"Flag {flag} not found in configuration.")


if __name__ == "__main__":
    main()
