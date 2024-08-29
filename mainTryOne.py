import os
import io

with open('D:', 'rb') as disk:
    data = disk.read(512)  # Read the first 512 bytes (a sector)
