#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <cstdint>
#include <cassert>
#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384
#define FNAME_CHAR_SIZE 1
#define FNAME_LENGTH 20

size_t get_file_size(const char* file_name)
{
    struct stat res;
    if (stat(file_name, &res) == 0)
        return res.st_size;

    return -1;
}

int compress_single_file(const char* source_path, const char* dest_path)
{
    FILE *source = fopen(source_path, "rb");
    FILE *dest = fopen(dest_path, "wb");

    if ((source == NULL) || (dest == NULL))
        return -1;

    int res_code;
    int flush;
    int compression_level = 9;
    unsigned have;
    z_stream stream;
    unsigned char input[CHUNK];
    unsigned char output[CHUNK];

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    res_code = deflateInit(&stream, compression_level);
    if (res_code != Z_OK)
        return res_code;

    do
    {
        stream.avail_in = fread(input, 1, CHUNK, source);
        if (ferror(source))
        {
            (void)deflateEnd(&stream);
            fclose(source);
            fclose(dest);

            return Z_ERRNO;
        }

        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = input;

        do{
            stream.avail_out = CHUNK;
            stream.next_out = output;

            res_code = deflate(&stream, flush);
            assert(res_code != Z_STREAM_ERROR);

            have = CHUNK - stream.avail_out;
            if ((fwrite(output, 1, have, dest) != have) || (ferror(dest)))
            {
                (void)deflateEnd(&stream);
                fclose(source);
                fclose(dest);

                return Z_ERRNO;
            }
        } while (stream.avail_out == 0);
        assert(stream.avail_in == 0);

    } while (flush != Z_FINISH);
    assert(res_code == Z_STREAM_END);

    (void)deflateEnd(&stream);
    fclose(source);
    fclose(dest);

    return Z_OK;
}

bool append_file(const char* file_path, const char file_name[FNAME_LENGTH], FILE* dest)
{
    FILE* source = fopen(file_path, "rb");
    if (!source)
        return false;

    size_t written = fwrite(file_name, FNAME_CHAR_SIZE, FNAME_LENGTH, dest);
    if (written != FNAME_LENGTH)
    {
        fclose(source);

        return false;
    }

    uint64_t source_size = get_file_size(file_path);
    written = fwrite(&source_size, sizeof(uint64_t), 1, dest);
    if (written != 1)
    {
        fclose(source);

        return false;
    }

    unsigned char *buffer = (unsigned char*)malloc(source_size);
    size_t read = fread(buffer, source_size, 1, source);
    if (read != 1)
    {
        fclose(source);
        return false;
    }
    written = fwrite(buffer, source_size, 1, dest);
    if (written != 1)
    {
        fclose(source);
        return false;
    }

    fclose(source);

    return true;
}

bool compress()
{
    int res_code;

    res_code = compress_single_file("./data/manifest.xml", "./data/res/manifest.cxml");
    if (res_code != Z_OK)
        return false;
    res_code = compress_single_file("./data/page_example.xml", "./data/res/page_example.cxml");
    if (res_code != Z_OK)
        return false;

    FILE *dest = fopen("./data/res/quest.dqf", "wb");
    if (dest == NULL)
        return false;
    bool res = append_file("./data/res/manifest.cxml", "manifest.cxml", dest);
    if (!res)
    {
        fclose(dest);
        return false;
    }
    res = append_file("./data/res/page_example.cxml", "page_1.cxml", dest);
    if (!res)
    {
        fclose(dest);
        return false;
    }
    fclose(dest);

    int deleted = remove("./data/res/manifest.cxml");
    if (deleted != 0)
        std::cout << "File delete error!" << std::endl;
    deleted = remove("./data/res/page_example.cxml");
    if (deleted != 0)
        std::cout << "File delete error!" << std::endl;

    return true;
}

bool decompress()
{
    FILE *source = fopen("./data/res/quest.dqf", "rb");
    if (!source)
        return false;
    uint64_t file_size;
    char file_name[FNAME_LENGTH];

    int read = fread(file_name, FNAME_CHAR_SIZE, FNAME_LENGTH, source);
    if (read != FNAME_LENGTH)
    {
        fclose(source);
        return false;
    }
    read = fread(&file_size, sizeof(uint64_t), 1, source);
    if (read != 1)
    {
        fclose(source);
        return false;
    }

    const char* prefix = "./data/res/";
    char path[strlen(file_name) + strlen(prefix) + 1];
    snprintf(path, sizeof(path), "%s%s", prefix, file_name);
    FILE* buffer_file = fopen(path, "wb");
    if (!buffer_file)
    {
        fclose(source);
        return false;
    }
    unsigned char* buffer = (unsigned char*)malloc(file_size);
    if (!buffer)
    {
        fclose(source);
        fclose(buffer_file);
        return false;
    }
    read = fread(buffer, file_size, 1, source);
    if (read != 1)
    {
        fclose(source);
        fclose(buffer_file);
        free(buffer);
        return false;
    }
    int written = fwrite(buffer, file_size, 1, buffer_file);
    if (written != 1)
    {
        fclose(source);
        fclose(buffer_file);
        free(buffer);
        return false;
    }

    fclose(source);
    fclose(buffer_file);
    free(buffer);

    std::cout << file_name << ", " << file_size << std::endl;

    return true;
}

int main()
{
    std::cout << "Mode:" << std::endl;
    int mode;
    std::cin>>mode;
    std::cout << "Output:" << std::endl;
    if (mode==1)
    {
        bool result = compress();
        std::cout << result << std::endl;

    }
    else if (mode==2)
        decompress();
    else
        std::cout << "Exit!" << std::endl;

    return 0;
}
