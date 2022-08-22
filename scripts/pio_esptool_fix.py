Import("env")

if env["PIOENV"] == "sol3dev":
    print(
        "Forcing 64MB flash for uploads until https://github.com/espressif/esptool/pull/769 is merged and available"
    )
    uploader_flags = env["UPLOADERFLAGS"]
    uploader_flags[uploader_flags.index("--flash_size") + 1] = "64MB"
    env.Replace(UPLOADERFLAGS=uploader_flags)
