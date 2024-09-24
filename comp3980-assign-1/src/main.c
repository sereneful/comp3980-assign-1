#include "../include/display.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define FILE_PERMISSIONS 0644
#define BUFFER_SIZE 1024

char upper_filter(char c);
char lower_filter(char c);
char null_filter(char c);

typedef char (*filter_func)(char);

static void display_file_contents(const char *filename, const char *description);

filter_func select_filter(const char *filter_name);

char upper_filter(char c)
{
    return (char)toupper((unsigned char)c);
}

char lower_filter(char c)
{
    return (char)tolower((unsigned char)c);
}

char null_filter(char c)
{
    return c;
}

filter_func select_filter(const char *filter_name)
{
    if(strcmp(filter_name, "upper") == 0)
    {
        return upper_filter;
    }
    if(strcmp(filter_name, "lower") == 0)
    {
        return lower_filter;
    }
    if(strcmp(filter_name, "null") == 0)
    {
        return null_filter;
    }
    return NULL;
}

// Function to display file contents
static void display_file_contents(const char *filename, const char *description)
{
    int     fd = open(filename, O_RDONLY | O_CLOEXEC);
    char    buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    if(fd < 0)
    {
        perror("Failed to open file for displaying contents");
        return;
    }

    printf("Contents of %s (%s):\n", filename, description);

    while((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        fwrite(buffer, 1, (size_t)bytes_read, stdout);
    }

    printf("\n");

    if(bytes_read < 0)
    {
        perror("Failed to read file");
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    int         input_fd        = -1;
    int         output_fd       = -1;
    const char *input_filename  = NULL;
    const char *output_filename = NULL;
    const char *filter_name     = NULL;
    filter_func filter          = NULL;
    int         opt;
    char        buffer[BUFFER_SIZE];
    ssize_t     bytes_read;

    while((opt = getopt(argc, argv, "i:o:f:")) != -1)
    {
        switch(opt)
        {
            case 'i':
                input_filename = optarg;
                break;
            case 'o':
                output_filename = optarg;
                break;
            case 'f':
                filter_name = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i input_file -o output_file -f filter\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(input_filename == NULL || output_filename == NULL || filter_name == NULL)
    {
        fprintf(stderr, "Usage: %s -i input_file -o output_file -f filter\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Display the content of input file before transformation
    display_file_contents(input_filename, "pre-transformation");

    filter = select_filter(filter_name);
    if(filter == NULL)
    {
        fprintf(stderr, "Invalid filter: %s. Choose from 'upper', 'lower', or 'null'.\n", filter_name);
        exit(EXIT_FAILURE);
    }

    // Input and output files are the same
    if(strcmp(input_filename, output_filename) == 0)
    {
        // Open input file and put contents into memory buffer
        input_fd = open(input_filename, O_RDONLY | O_CLOEXEC);
        if(input_fd < 0)
        {
            perror("Failed to open input file");
            exit(EXIT_FAILURE);
        }

        // Read the entire content of the file into memory (buffer)
        ssize_t total_bytes  = 0;
        char   *file_content = NULL;

        // Allocate memory for file content dynamically
        file_content = (char *)malloc(BUFFER_SIZE);
        if(!file_content)
        {
            perror("Failed to allocate memory for file content");
            close(input_fd);
            exit(EXIT_FAILURE);
        }

        while((bytes_read = read(input_fd, buffer, BUFFER_SIZE)) > 0)
        {
            file_content = realloc(file_content, total_bytes + bytes_read);
            if(!file_content)
            {
                perror("Failed to allocate memory while reading");
                close(input_fd);
                exit(EXIT_FAILURE);
            }
            memcpy(file_content + total_bytes, buffer, bytes_read);
            total_bytes += bytes_read;
        }

        if(bytes_read < 0)
        {
            perror("Failed to read input file");
            close(input_fd);
            free(file_content);
            exit(EXIT_FAILURE);
        }
        close(input_fd);

        for(ssize_t i = 0; i < total_bytes; i++)
        {
            file_content[i] = filter(file_content[i]);
        }

        // Open the output file (same as input) for writing and truncate it
        output_fd = open(output_filename, O_WRONLY | O_TRUNC | O_CLOEXEC, FILE_PERMISSIONS);
        if(output_fd < 0)
        {
            perror("Failed to open output file");
            free(file_content);
            exit(EXIT_FAILURE);
        }

        // Write the transformed content back to the same file
        if(write(output_fd, file_content, total_bytes) != total_bytes)
        {
            perror("Failed to write transformed content to file");
            close(output_fd);
            free(file_content);
            exit(EXIT_FAILURE);
        }

        free(file_content);
        close(output_fd);
    }
    else
    {
        // Input and output files are different
        input_fd = open(input_filename, O_RDONLY | O_CLOEXEC);
        if(input_fd < 0)
        {
            perror("Failed to open input file");
            exit(EXIT_FAILURE);
        }

        output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, FILE_PERMISSIONS);
        if(output_fd < 0)
        {
            perror("Failed to create output file");
            close(input_fd);
            exit(EXIT_FAILURE);
        }

        // Read the input file, transform it, and write it to the output file
        while((bytes_read = read(input_fd, &buffer, BUFFER_SIZE)) > 0)
        {
            for(ssize_t i = 0; i < bytes_read; i++)
            {
                buffer[i] = filter(buffer[i]);
            }
            if(write(output_fd, buffer, bytes_read) != bytes_read)
            {
                perror("Failed to write to output file");
                close(input_fd);
                close(output_fd);
                exit(EXIT_FAILURE);
            }
        }

        if(bytes_read < 0)
        {
            perror("Failed to read input file");
        }

        close(input_fd);
        close(output_fd);
    }

    // Display the content of output file after transformation
    display_file_contents(output_filename, "post-transformation");

    return 0;
}
