Import("env")
import platform
import sys

if platform.system() == "Windows":
    # Let PlatformIO know we need MinGW
    env.Replace(
        CC="gcc",
        CXX="g++",
        AR="ar",
        RANLIB="ranlib"
    )