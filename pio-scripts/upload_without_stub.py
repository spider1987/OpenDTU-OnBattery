Import("env")


uploader_flags = env.get("UPLOADERFLAGS", [])
if "write_flash" in uploader_flags and "--no-stub" not in uploader_flags:
    uploader_flags.insert(uploader_flags.index("write_flash"), "--no-stub")
