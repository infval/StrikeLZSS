#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>

#include <assert.h>


#define VERSION_STR "v1.2.0"


// Offset: [0, 2047]
#define WINDOW_SIZE     0x800
#define WINDOW_INIT_POS 0x7EE
// Sequence: [3, 18]
#define MAX_SEQ         18
// Urban Strike: $00FF26F2
uint8_t window[WINDOW_SIZE];

//
// Compress / Decompress
//
size_t LZSS_Decompress(const uint8_t* src, size_t src_max_len, uint8_t* dst, uint32_t dsize);
size_t LZSS_Compress(const uint8_t* src, size_t src_len, uint8_t* dst, uint8_t* custom_lens);
size_t LZSS_GetCompressedMaxSize(size_t src_len);

// Urban Strike: ROM [$762A, $76E2]
size_t LZSS_Decompress(const uint8_t* src, size_t src_max_len, uint8_t* dst, uint32_t dsize)
{
    size_t wpos = WINDOW_INIT_POS;
    size_t dpos = 0;
    size_t spos = 0;
    uint16_t control_byte = 0;

    if (dsize == 0) {
        return 0;
    }

    for (;;) {
        control_byte >>= 1;
        if (!(control_byte & 0x0100)) {
            // Bad data
            if (src_max_len == 0) {
                return (size_t)-1;
            }
            src_max_len--;

            control_byte = src[spos++] | 0xFF00;
        }

        if (control_byte & 1) {
            // Bad data
            if (src_max_len == 0) {
                return (size_t)-1;
            }
            src_max_len--;

            uint8_t raw_byte = src[spos++];
            dst[dpos++] = raw_byte;
            dsize--;
            if (dsize == 0) {
                return spos;
            }
            window[wpos] = raw_byte;
            wpos = (wpos + 1) & (WINDOW_SIZE - 1);
        }
        else {
            // Bad data
            if (src_max_len < 2) {
                return (size_t)-1;
            }
            src_max_len -= 2;

            size_t offset = src[spos] | ((src[spos + 1] & 0xF0) << 4);
            size_t length = (src[spos + 1] & 0x0F) + 3;
            spos += 2;
            while (length--) {
                uint8_t raw_byte = window[offset];
                offset = (offset + 1) & (WINDOW_SIZE - 1);
                dst[dpos++] = raw_byte;
                dsize--;
                if (dsize == 0) {
                    return spos;
                }
                window[wpos] = raw_byte;
                wpos = (wpos + 1) & (WINDOW_SIZE - 1);
            }
        }
    }
    return 0;
}

size_t LZSS_Compress(const uint8_t* src, size_t src_len, uint8_t* dst, uint8_t* custom_lens)
{
    size_t spos = 0, dpos = 0;
    uint8_t cb = 0;
    int8_t cb_bit = 0;
    size_t cb_pos = 0;

    dst[dpos++] = 0x00;
    while (spos < src_len) {
        size_t offset = 0;
        size_t length = 0;

        for (intptr_t i = spos - 1; (i >= 0) && (i >= (intptr_t)spos - WINDOW_SIZE); i--) {
        //for (size_t i = (spos >= WINDOW_SIZE ? (intptr_t)spos - WINDOW_SIZE : 0); i < spos; i++) {
            if (src[i] == src[spos]) {
                size_t cur_len = 0;
                do {
                    cur_len++;
                } while ((cur_len < MAX_SEQ)
                    && (spos + cur_len < src_len)
                    && src[i + cur_len] == src[spos + cur_len]);

                if (cur_len > length) {
                    offset = (WINDOW_INIT_POS + i) & (WINDOW_SIZE - 1);
                    length = cur_len;
                    if (length >= MAX_SEQ) {
                        break;
                    }
                }
            }
        }

        if (custom_lens != NULL) {
            length = custom_lens[spos];
        }
        if (length >= 3) {
            size_t c = offset;
            c = (c & 0x00FF) | ((c & 0x0F00) << 4);
            c |= (length - 3) << 8;
            dst[dpos++] = c & 0xFF;
            dst[dpos++] = (uint8_t)(c >> 8);
            spos += length;
        }
        else {
            dst[dpos++] = src[spos++];
            cb |= 1 << cb_bit;
        }

        cb_bit++;
        if (cb_bit > 7) {
            dst[cb_pos] = cb;
            cb = 0x00;
            cb_bit = 0;
            cb_pos = dpos;
            dst[dpos++] = 0x00;
        }
    }

    if (cb_bit == 0) {
        dpos--;
    }
    else {
        dst[cb_pos] = cb;
    }
    return dpos;
}

size_t LZSS_GetCompressedMaxSize(size_t src_len)
{
    return 1 + src_len + src_len / 8;
}

//
// Ultra
//
uint64_t LZSS_CalcLength(const uint8_t* lens, size_t src_len);
size_t   LZSS_GetVars(const uint8_t* lens, size_t src_len, size_t* vars);
size_t   LZSS_CompressUltra(const uint8_t* src, size_t src_len, uint8_t* dst);

uint64_t LZSS_CalcLength(const uint8_t* lens, size_t src_len)
{
    uint64_t count1 = 0;
    uint64_t count2 = 0;
    size_t i = 0;
    while (i < src_len) {
        if (lens[i] >= 3) {
            count2 += 1;
            i += lens[i];
        }
        else {
            count1 += 1;
            i += 1;
        }
    }
    return ((count2 * 2 + count1) << 3) + (count2 + count1); // Bits
}

size_t LZSS_GetVars(const uint8_t* lens, size_t src_len, size_t* vars)
{
    // (1st) index       greater than 16
    //       |           |
    //       V           V
    // ... 1 16 15 14 13 18 17 16 15 ...
    //       |        |
    //       *--------*
    // (2nd) length
    size_t count = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (lens[i] >= 3) {
            for (size_t j = 1; j < lens[i]; j++) {
                if ((i + j < src_len) && (lens[i + j] > lens[i])) {
                    if (vars == NULL) {
                        count += 2;
                    }
                    else {
                        vars[count++] = i;
                        vars[count++] = j;
                    }
                }
            }
        }
    }
    return count;
}

size_t LZSS_CompressUltra(const uint8_t* src, size_t src_len, uint8_t* dst)
{
    size_t spos = 0;

    uint8_t* lens = (uint8_t*)malloc(src_len);
    if (lens == NULL) {
        fprintf(stderr, "Error: malloc()");
        return (size_t)-1;
    }

    while (spos < src_len) {
        size_t length = 1;

        for (intptr_t i = spos - 1; (i >= 0) && (i >= (intptr_t)spos - WINDOW_SIZE); i--) {
            if (src[i] == src[spos]) {
                size_t cur_len = 0;
                do {
                    cur_len++;
                } while ((cur_len < MAX_SEQ)
                    && (spos + cur_len < src_len)
                    && src[i + cur_len] == src[spos + cur_len]);

                if (cur_len > length) {
                    length = cur_len;
                    if (length >= MAX_SEQ) {
                        break;
                    }
                }
            }
        }
        lens[spos] = (uint8_t)length;
        spos++;
    }

    uint64_t base_comp = LZSS_CalcLength(lens, src_len);
    uint64_t base_comp_bytes = base_comp / 8 + (base_comp & 3 ? 1 : 0);
    printf("Average comp.size: %10" PRIu64 " | 0x%08" PRIX64 "\n", base_comp_bytes, base_comp_bytes);

    size_t vars_len = LZSS_GetVars(lens, src_len, NULL);
    size_t* vars = (size_t*)malloc(vars_len * sizeof(size_t));
    if (vars == NULL) {
        fprintf(stderr, "Error: malloc()");
        return (size_t)-1;
    }
    LZSS_GetVars(lens, src_len, vars);

    uint64_t prev_result = base_comp;
    for (size_t i = 0; i < vars_len; i += 2) {
        // [i], [i + 1] or [(vars_len - 2) - i], [(vars_len - 2) - i + 1] (better)
        size_t index = vars[(vars_len - 2) - i];
        uint8_t new_len = (uint8_t)vars[(vars_len - 2) - i + 1];

        uint8_t prev_len = lens[index];
        lens[index] = new_len;
        uint64_t res = LZSS_CalcLength(lens, src_len);
        if (res < prev_result) {
            prev_result = res;
        }
        else {
            lens[index] = prev_len;
        }
    }

    size_t dpos = LZSS_Compress(src, src_len, dst, lens);

    free(vars);
    free(lens);
    return dpos;
}

//
// Misc
//
long          GetFileSize(FILE* file);
uint32_t      Read_u32be(const uint8_t* src);
void          Write_u32be(uint32_t n, uint8_t* dst);
unsigned long Parse_ulong(const char* str);

// Max: 2 GB
long GetFileSize(FILE* file)
{
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

uint32_t Read_u32be(const uint8_t* src)
{
    return (src[0] << 24)
         | (src[1] << 16)
         | (src[2] <<  8)
         | (src[3] <<  0);
}

void Write_u32be(uint32_t n, uint8_t* dst)
{
    dst[0] = (n >> 24) & 0xFF;
    dst[1] = (n >> 16) & 0xFF;
    dst[2] = (n >>  8) & 0xFF;
    dst[3] = (n >>  0) & 0xFF;
}

unsigned long Parse_ulong(const char* str)
{
    char s[10 + 1] = { 0 }; // Max. 0xFFFFFFFF or 4294967295
    strncpy(s, str, 10);
    if (s[0] == '0' && toupper(s[1]) == 'X') {
        return strtoul(s + 2, NULL, 16);
    }
    return strtoul(s, NULL, 10);
}

//
// Arguments
//

typedef enum {
    ACTION_COMPRESS,
    ACTION_DECOMPRESS
} Action;

typedef struct {
    const char* input_path;
    const char* output_path;
    Action action;
    uint32_t file_pos;
    bool is_ultra;
} Args;

const char* arguments[] = { "c", "d", "p", "nu", NULL };

void argparse(int argc, char* argv[], Args* args)
{
    args->input_path  = NULL;
    args->output_path = NULL;
    args->action      = ACTION_COMPRESS;
    args->file_pos    = 0;
    args->is_ultra    = true;
    size_t pos = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            size_t index = 0;
            for (; arguments[index] != NULL; index++) {
                if (!strcmp(arguments[index], argv[i] + 1)) {
                    break;
                }
            }
            switch (index) {
            case 0:
                args->action = ACTION_COMPRESS;
                break;
            case 1:
                args->action = ACTION_DECOMPRESS;
                break;
            case 2:
                if (i + 1 < argc) {
                    args->file_pos = Parse_ulong(argv[i + 1]);
                    i++;
                }
                break;
            case 3:
                args->is_ultra = false;
                break;
            default:
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                break;
            }
        }
        else {
            switch (pos) {
            case 0:
                args->input_path = argv[i];
                break;
            case 1:
                args->output_path = argv[i];
                break;
            default:
                fprintf(stderr, "Unknown argument: %s\n", argv[i]);
                break;
            }
            pos++;
        }
    }
}

int main(int argc, char* argv[])
{
    // Arguments

    Args args = { 0 } ;
    argparse(argc, argv, &args);

    if (args.input_path == NULL || args.output_path == NULL) {
        printf("LZSS compressor - Desert/Jungle/Urban Strike (Mega Drive) || " VERSION_STR " by infval\n"
            "usage: %s INPUT OUTPUT [-c | -d] [-p file_pos] [-nu]\n"
            "positional arguments:\n"
            "  INPUT          input file; if decompress, first 4 bytes (Big-Endian): uncompressed size\n"
            "  OUTPUT         output file\n"
            "optional arguments:\n"
            "  -c             compress (default)\n"
            "  -d             decompress\n"
            "  -p file_pos    start file position (default: 0)\n"
            "  -nu            average compression (faster)\n"
            , argv[0]);
        return 0;
    }

    // Input

    FILE* finput = fopen(args.input_path, "rb");
    if (finput == NULL) {
        fprintf(stderr, "Can't open: %s", args.input_path);
        return 1;
    }
    size_t file_size = GetFileSize(finput);
    if (file_size == (size_t)-1) {
        fprintf(stderr, "Error: GetFileSize()");
        return 1;
    }
    uint8_t* source = (uint8_t*)malloc(file_size);
    if (source == NULL) {
        fprintf(stderr, "Error: malloc()");
        return 1;
    }
    size_t read_bytes = fread(source, 1, file_size, finput);
    if (read_bytes != file_size) {
        fprintf(stderr, "Can't open: %s", args.input_path);
        return 1;
    }
    fclose(finput);

    // Actions

    const size_t header_size = 4;

    uint8_t* dest = NULL;
    size_t dsize = 0;
    size_t usize = 0;
    size_t csize = 0;
    if (args.action == ACTION_COMPRESS) {
        if (args.file_pos > file_size) {
            fprintf(stderr, "Error: file_pos > input file size");
            return 1;
        }

        usize = file_size - args.file_pos;
        size_t maxsize = LZSS_GetCompressedMaxSize(usize) + header_size;
        dest = (uint8_t*)malloc(maxsize);
        if (dest == NULL) {
            fprintf(stderr, "Error: malloc()");
            return 1;
        }

        Write_u32be((uint32_t)usize, dest);
        if (args.is_ultra) {
            dsize = LZSS_CompressUltra(source + args.file_pos, usize, dest + header_size);
        }
        else {
            dsize = LZSS_Compress(source + args.file_pos, usize, dest + header_size, NULL);
        }
        if (dsize == (size_t)-1) {
            fprintf(stderr, "Error: LZSS_Compress*()");
            return 1;
        }
        csize = dsize;
        dsize += header_size;
        assert(dsize <= maxsize);
    }
    else if (args.action == ACTION_DECOMPRESS) {
        if (args.file_pos > file_size - header_size) {
            fprintf(stderr, "Error: file_pos > input file size");
            return 1;
        }

        usize = Read_u32be(source + args.file_pos);
        dest = (uint8_t*)malloc(usize);
        if (dest == NULL) {
            fprintf(stderr, "Error: malloc()");
            return 1;
        }

        csize = LZSS_Decompress(source + args.file_pos + header_size, file_size - args.file_pos - header_size, dest, (uint32_t)usize);
        if (csize == (size_t)-1) {
            fprintf(stderr, "Error: LZSS_Decompress()");
            return 1;
        }
        dsize = usize;
    }
    printf("Uncompressed size: %10" PRIuPTR " | 0x%08" PRIXPTR "\n", usize, usize);
    printf("Compressed size  : %10" PRIuPTR " | 0x%08" PRIXPTR, csize, csize);

    // Output

    FILE* fout = fopen(args.output_path, "wb");
    if (fout == NULL) {
        fprintf(stderr, "Can't open: %s", args.output_path);
        return 1;
    }
    size_t write_bytes = fwrite(dest, 1, dsize, fout);
    if (write_bytes != dsize) {
        fprintf(stderr, "Can't write: %s", args.output_path);
        return 1;
    }
    fclose(fout);

    free(dest);
    free(source);
    return 0;
}
