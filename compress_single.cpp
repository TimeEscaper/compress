// На основе статьи: http://www.zlib.net/zlib_how.html
// Сжатие и распаковка одного файла с использованием  ZLib
// Следующий шаг - сжатие нескольких файлов в один и его распаковка
// ZLib использует алгоритм Deflate, который является комбинацией
// алгоритмов LZ77 и алгоритма Хаффмана

#include <stdio.h>

#include <iostream>
#include <string>
#include <cassert>

#include <zlib.h>

//--- Хак для подключения нужных библиотек
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif //---

// Размер буфера, необходимый zlib для работы с данными.
// Чем больше размер буффера, тем эффективнее работают алгоритмы.
// Желательно брать буффер размером 128 или 256 Кб, если это позволяют ресурсы.
#define CHUNK 16384

// Todo: сжатие файла
// Input:
// source_path - путь к сжимаемому файлу
// dest_path - путь к выходному сжатому файлу
// Output: код результата zlib
int compress(const char* source_path, const char* dest_path)
{
    FILE *source = fopen(source_path, "rb");
    FILE *dest = fopen(dest_path, "wb");

    if ((source == NULL) || (dest == NULL))
        return -1;

    int res_code; // Возвращаемый функциями из zlib код результата
    int flush; // Текущее состояние данных для функции deflate()
    // Степень сжатия. 0 - минимум, 9 - максимум, -1 - соотношение по умолчанию.
    // Скорость оюратно пропорциональна степени сжатия!
    int compression_level = 9;
    unsigned have; // Размер данных, возвращённых функцей deflate()
    z_stream stream; // Структура для обмена данными в алгоримах zlib
    // Буферы для функции deflate()
    unsigned char input[CHUNK];
    unsigned char output[CHUNK];

    stream.zalloc = Z_NULL; // Выделение памяти в zlib. Z_NULL - оставить по умолчанию.
    stream.zfree = Z_NULL; // Освобождение памяти в zlib. Z_NULL - оставить по умолчанию.
    stream.opaque = Z_NULL; // Прочие внутренние настройки в zlib. Z_NULL - оставить по умолчанию.
    res_code = deflateInit(&stream, compression_level); // Инициализация для сжатия
    if (res_code != Z_OK)
        return res_code;

    do
    {
        // В avail_in записывается число прочитанных байт (указатель на них записывается в next_in)
        stream.avail_in = fread(input, 1, CHUNK, source);
        if (ferror(source))
        {
            (void)deflateEnd(&stream);
            fclose(source);
            fclose(dest);

            return Z_ERRNO;
        }

        // Проверка достижения конца файла.
        // Если достигнут конец файла, то установим flush значение Z_FINISH -
        // - для deflate() это означает, что работа ведется с последним блоком данных
        // файла, который необоходимо сжать, и помимо сжатия необходимо вычислить контрольную сумму и прочее.
        // Иначе, Z_NO_FLUSH показывает, что работа ведётся с промежуточными данными
        // несжатого файла.
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = input; // Указатель на считанные данные (см.упоминание выше)

        do{
            stream.avail_out = CHUNK; // Число доступных для выходных данных байт
            stream.next_out = output; // Укзатель на них

            // deflate() - непосредственно само сжатие данных.
            // deflate() берёт столько байт из avail_in байт в next_in, сколько может
            // обработать, из записывает avail_out байт в next_out,
            // после чего все указатели и счётчики обновляются.
            res_code = deflate(&stream, flush);
            assert(res_code != Z_STREAM_ERROR); // Z_STREAM_ERROR - ошибка в структуре z_stream

            // Вычисление количества сжатых данных и их запись в выходной файл
            have = CHUNK - stream.avail_out;
            if ((fwrite(output, 1, have, dest) != have) || (ferror(dest)))
            {
                (void)deflateEnd(&stream);
                fclose(source);
                fclose(dest);

                return Z_ERRNO;
            }
        } while (stream.avail_out == 0); // Условие выхода их цикла - все данные должны быть сжаты,
                                        // то есть в next_out уже нечего заисывать.
        assert(stream.avail_in == 0);

    } while (flush != Z_FINISH); // Условие выхода - достижение конца файла, при этом flush
                                // будет иметь значение Z_FINISH (см. выше).
    assert(res_code == Z_STREAM_END);

    (void)deflateEnd(&stream);
    fclose(source);
    fclose(dest);

    return Z_OK;
}

// Todo: распаковка сжатого файла
// Input:
// source_path - путь к сжатому файлу
// dest_path - путь к выходному распакованному файлу
// Output: код результата zlib
// В целом, функция работает почти так же, как и функция compress()
int decompress(const char* source_path, const char* dest_path)
{
    FILE *source = fopen(source_path, "rb");
    FILE *dest = fopen(dest_path, "wb");

    if ((source == NULL) || (dest == NULL))
        return -1;

    int res_code, flush;
    unsigned have;
    z_stream stream;
    unsigned char input[CHUNK];
    unsigned char output[CHUNK];

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
    res_code = inflateInit(&stream);
    if(res_code != Z_OK)
        return res_code;

    do
    {
        stream.avail_in = fread(input, 1, CHUNK, source);
        if (ferror(source))
        {
            (void)inflateEnd(&stream);
            fclose(source);
            fclose(dest);

            return Z_ERRNO;
        }

        if (stream.avail_in == 0)
            break;
        stream.next_in = input;

        do
        {
            stream.avail_out = CHUNK;
            stream.next_out = output;

            res_code = inflate(&stream, Z_NO_FLUSH);
            assert(res_code != Z_STREAM_ERROR);
            switch (res_code) {
                case Z_NEED_DICT:
                    res_code = Z_DATA_ERROR; // Нет словаря - чатсный случай повреждения данных
                    break;

                case Z_DATA_ERROR: break; // Данные повреждены

                case Z_MEM_ERROR: // Ошибка в работе с памятью в zlib
                    (void)inflateEnd(&stream);
                    fclose(source);
                    fclose(dest);

                    return res_code;
            }

            have = CHUNK - stream.avail_out;
            if ((fwrite(output, 1, have, dest) != have) || ferror(dest))
            {
                (void)inflateEnd(&stream);
                fclose(source);
                fclose(dest);

                return Z_ERRNO;
            }

        } while (stream.avail_out == 0);

    } while (res_code != Z_STREAM_END); // Условие выхода из цикла - все данные в z_stream обработаны,
                                        // контрольные суммы и прочие проверки выполнены.

    (void)inflateEnd(&stream);
    fclose(source);
    fclose(dest);

    return res_code == Z_OK;

}

int main()
{
    int res = compress("./data/manifest.xml", "./data/manifest_compressed.cxml");
    std::cout << "Compression:" << res << std::endl;
    res = decompress("./data/manifest_compressed.cxml", "./data/manifest_decompressed.xml");
    std::cout << "Decompression: " << res << std::endl;

    return 0;
}
