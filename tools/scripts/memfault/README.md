# Memfault Scripts

All scripts were written using python 3.8. Backward compatibility not ensured.
The following modules should already be installed via conda environment, but please install them if not:
- python-dotenv
- requests
- gitpython

To install within the conda environment,
```$ conda activate xMoonJelly_fw && pip install (module)```

Make sure your environment variables have been set up before running these scripts.

----------
#### `memfault_elf_uploader.py`
Wrapper script to use `memfault` cli command within a conda env in CLion. Automagically uploads the .elf living in your build folder to Memfault.
