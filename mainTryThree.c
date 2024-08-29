#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>  // Windows-specific headers
#include <winioctl.h> // For IOCTL_DISK_GET_LENGTH_INFO
#include <errno.h>    // For error handling

// JPEG file signatures
const uint8_t jpeg_header[] = { 0xFF, 0xD8, 0xFF };
const uint8_t jpeg_footer[] = { 0xFF, 0xD9 };

// Function to check if data matches the JPEG header
int is_jpeg_header(const uint8_t *buffer) {
    return (buffer[0] == jpeg_header[0] && buffer[1] == jpeg_header[1] && buffer[2] == jpeg_header[2]);
}

// Function to check if data matches the JPEG footer
int is_jpeg_footer(const uint8_t *buffer) {
    return (buffer[0] == jpeg_footer[0] && buffer[1] == jpeg_footer[1]);
}

// Function to get the size of a physical disk on Windows
long long get_disk_size(const char *disk_path) {
    HANDLE hDisk = CreateFile(disk_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        printf("Error opening disk: %ld\n", GetLastError());
        return -1;
    }

    GET_LENGTH_INFORMATION diskLength;
    DWORD bytesReturned;
    
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &diskLength, sizeof(diskLength), &bytesReturned, NULL)) {
        printf("Error getting disk size: %ld\n", GetLastError());
        CloseHandle(hDisk);
        return -1;
    }

    CloseHandle(hDisk);
    return diskLength.Length.QuadPart;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <raw_disk_or_image>\n", argv[0]);
        return 1;
    }

    // Get the size of the disk using Windows-specific API
    long long total_size = get_disk_size(argv[1]);
    if (total_size == -1) {
        printf("Failed to retrieve disk size.\n");
        return 1;
    }

    printf("Starting recovery...\n");
    printf("Total disk/image size: %lld bytes\n", total_size);

    // Open the raw disk or disk image
    FILE *disk = fopen(argv[1], "rb");
    if (disk == NULL) {
        perror("Error opening disk");
        return 1;
    }

    // Buffer for reading data
    uint8_t buffer[512];
    int file_counter = 0;
    long long bytes_read_total = 0;
    FILE *recovered_file = NULL;

    // Scan through the disk
    while (1) {
        // Try reading 512 bytes from the disk
        size_t bytes_read = fread(buffer, sizeof(uint8_t), 512, disk);
        bytes_read_total += bytes_read;

        // Calculate and print progress
        double progress = (double)bytes_read_total / total_size * 100.0;
        printf("\rProgress: %.2f%%", progress);
        fflush(stdout);  // Flush the output to ensure it's printed immediately

        // Handle read errors
        if (bytes_read != 512) {
            if (feof(disk)) {
                // End of file reached, break the loop
                printf("\nEnd of disk/image reached.\n");
                break;
            }
            if (ferror(disk)) {
                // Read error, attempt to clear the error and skip the bad sector
                perror("\nError reading from disk");
                clearerr(disk);
                fseek(disk, 512, SEEK_CUR);  // Skip one sector and continue
                printf("\nSkipping bad sector...\n");
                continue;
            }
        }

        // Check for JPEG header
        if (is_jpeg_header(buffer)) {
            // Close the previous file if it's open
            if (recovered_file != NULL) {
                fclose(recovered_file);
                printf("\nRecovered JPEG file: recovered_%03d.jpg\n", file_counter - 1);
            }

            // Create a new file for recovered JPEG
            char filename[20];
            sprintf(filename, "recovered_%03d.jpg", file_counter++);
            recovered_file = fopen(filename, "wb");
            if (recovered_file == NULL) {
                perror("\nError creating recovered file");
                fclose(disk);
                return 1;
            }

            printf("\nJPEG header found, creating %s...\n", filename);
            fwrite(buffer, sizeof(uint8_t), bytes_read, recovered_file);
        } 
        // Continue writing data to the open file if it's a JPEG
        else if (recovered_file != NULL) {
            fwrite(buffer, sizeof(uint8_t), bytes_read, recovered_file);

            // Check for JPEG footer
            if (is_jpeg_footer(buffer + bytes_read - 2)) {
                fclose(recovered_file);
                recovered_file = NULL;
                printf("\nJPEG file recovery completed.\n");
            }
        }
    }

    // Close any remaining files
    if (recovered_file != NULL) {
        fclose(recovered_file);
        printf("\nRecovered JPEG file: recovered_%03d.jpg\n", file_counter - 1);
    }

    fclose(disk);
    printf("\nRecovery completed!\n");
    return 0;
}
