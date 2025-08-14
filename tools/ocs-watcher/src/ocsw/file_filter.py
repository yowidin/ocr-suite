from pathlib import Path


def collect_by_extension(directory: Path, extensions: list[str]) -> list[Path]:
    file_paths = []
    base_path = Path(directory)

    for ext in extensions:
        for candidate in base_path.rglob(f'*{ext}'):
            if candidate.is_file():
                file_paths.append(candidate.resolve())

    return file_paths


if __name__ == "__main__":
    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument("-d", "--dir", type=Path, required=True, help="Directory to scan")
    parser.add_argument("extension", nargs='+', type=str, help="File extensions")

    args = parser.parse_args()

    files = collect_by_extension(args.dir, args.extension)
    for file in files:
        print(file)
