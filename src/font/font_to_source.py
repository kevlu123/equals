import pathlib

entries = [
    # (filename without extension, variable name)
    #("Roboto-Regular", "ROBOTO_REGULAR_FONT"),
    ("JetBrainsMono-Regular", "JET_BRAINS_MONO_FONT"),
]

cur_dir = pathlib.Path(__file__).parent

for file, var in entries:
    input_filename = cur_dir / f"{file}.ttf"
    output_source_filename = cur_dir / f"{file}.c"
    output_header_filename = cur_dir / f"{file}.h"

    print(f"Processing {input_filename}")
    with open(input_filename, "rb") as f:
        data = f.read()
    
    data_len = len(data)
    array_data = ",".join(str(b) for b in data)
    src = f"const char {var}[{data_len}] = {{{array_data}}};"
    header = f"extern \"C\" {{ extern const char {var}[{data_len}]; }}"

    with open(output_source_filename, "w") as f:
        f.write(src)
        print(f"Written {output_source_filename}")
    with open(output_header_filename, "w") as f:
        f.write(header)
        print(f"Written {output_header_filename}")
