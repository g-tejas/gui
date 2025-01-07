#pragma once

#include <cassert>
#include <csignal>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <zlib.h>

/*
 * Ghostty has no planned support for animation. Animations are implemented `client-side`.
 * See how `terminal-doom` does it.
 * Keys and what they mean
 * 'z=' animation related. defines the gap for frame, in milliseconds, before next frame
 * NOTE: we must define z= for frames that we want to be shown!
 * 'i=' is the image id. Can be any 32 bit number. all frames in one animation belong to one image.
 */

#define CSI "\x1B["
#define ESC "\x1B"

const uint8_t base64enc_tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(size_t in_len, const uint8_t *in, size_t out_len, char *out) {
    uint ii, io;
    uint_least32_t v;
    uint rem;

    for (io = 0, ii = 0, v = 0, rem = 0; ii < in_len; ii++) {
        uint8_t ch;
        ch = in[ii];
        v = (v << 8) | ch;
        rem += 8;
        while (rem >= 6) {
            rem -= 6;
            if (io >= out_len)
                return -1; /* truncation is failure */
            out[io++] = static_cast<char>(base64enc_tab[(v >> rem) & 63]);
        }
    }
    if (rem) {
        v <<= (6 - rem);
        if (io >= out_len)
            return -1; /* truncation is failure */
        out[io++] = static_cast<char>(base64enc_tab[v & 63]);
    }
    while (io & 3) {
        if (io >= out_len)
            return -1; /* truncation is failure */
        out[io++] = '=';
    }
    if (io >= out_len)
        return -1; /* no room for null terminator */
    out[io] = 0;
    return io;
}

void restore_terminal();

static struct termios orig_termios;

// Signal handler for Ctrl-C
static void signal_handler(int signum) {
    restore_terminal();
    exit(130); // Exit immediately with code 130 (128 + SIGINT)
}

void setup_terminal() {
    // Set up signal handler for Ctrl-C
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    std::cout << CSI << "s";      // save cursor
    std::cout << CSI << "?1049h"; // enter alt screen
    std::cout << CSI << "H";      // move cursor to the top left
    std::cout << CSI << "?25l";   // hide cursor
    std::cout.flush();
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    std::cout << CSI << "?25h";   // show cursor
    std::cout << CSI << "?1049l"; // exit alt screen
    std::cout << CSI << "u";      // restore cursor
    std::cout.flush();
}

auto kitty_send_command(const std::string &cmd_str, const uint8_t *payload_data = nullptr,
                        size_t payload_size = 0) -> size_t {
    constexpr size_t chunk_limit = 4096;

    if (!payload_data || payload_size == 0) {
        // Just send the command without payload
        std::cout << ESC << "_G" << cmd_str << "\\";
        std::cout.flush();
        return 0;
    }

    // Prepare base64 data
    size_t base64_size = ((payload_size + 2) / 3) * 4;
    auto base64_data = (uint8_t *)malloc(base64_size + 1);
    if (!base64_data) {
        std::cerr << "Failed to allocate memory for base64 encoding\n";
        return 0;
    }

    int ret =
        base64_encode(payload_size, payload_data, base64_size + 1, (char *)base64_data);
    if (ret < 0) {
        std::cerr << "Error: base64_encode failed: ret=" << ret << "\n";
        free(base64_data);
        return 0;
    }

    // Send data in chunks
    size_t sent_bytes = 0;
    std::size_t chunks = 0;
    while (sent_bytes < base64_size) {
        size_t chunk_size = std::min(chunk_limit, base64_size - sent_bytes);
        bool is_last_chunk = (sent_bytes + chunk_size >= base64_size);

        if (sent_bytes == 0) {
            // First chunk includes the command
            std::cout << ESC << "_G" << cmd_str << ",m=" << (is_last_chunk ? "0" : "1")
                      << ";";
        } else {
            // Continuation chunks
            std::cout << ESC << "_Gm=" << (is_last_chunk ? "0" : "1") << ";";
        }

        std::cout.write((char *)base64_data + sent_bytes, chunk_size);
        std::cout << ESC << "\\";
        std::cout.flush();

        chunks++;

        sent_bytes += chunk_size;
    }

    free(base64_data);
    return base64_size;
}

// int main(int argc, char **argv) {
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <gif_path>" << std::endl;
//         return 1;
//     }
//     const char *gif_path = argv[1];

//     std::this_thread::sleep_for(std::chrono::seconds(1));

//     setup_terminal();

//     std::ifstream file(gif_path, std::ios::binary | std::ios::ate);
//     if (!file) {
//         std::cerr << "Failed to open file: " << gif_path << std::endl;
//         restore_terminal();
//         return EXIT_FAILURE;
//     }

//     std::streamsize size = file.tellg();
//     file.seekg(0, std::ios::beg);

//     std::vector<unsigned char> buffer(size);
//     if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
//         std::cerr << "Failed to read file: " << gif_path << std::endl;
//         restore_terminal();
//         return EXIT_FAILURE;
//     }

//     int *delays = nullptr;
//     int width, height, frames, channels;
//     unsigned char *data = stbi_load_gif_from_memory(buffer.data(), size, &delays, &width,
//                                                     &height, &frames, &channels, 4);

//     if (!data) {
//         std::cerr << "Failed to load GIF: " << gif_path << std::endl;
//         restore_terminal();
//         return EXIT_FAILURE;
//     }

//     for (int i = 0; i < frames; i++) {
//         // Each frame is width * height * 4 bytes (RGBA)
//         size_t frame_size = width * height * 4;
//         unsigned char *frame_data = data + (i * frame_size);

//         // Compress the frame data using zlib
//         uLongf compressed_size = compressBound(frame_size);
//         unsigned char *compressed_data = (unsigned char *)malloc(compressed_size);
//         if (compress2(compressed_data, &compressed_size, frame_data, frame_size,
//                       Z_BEST_COMPRESSION) != Z_OK) {
//             std::cerr << "Failed to compress frame data" << std::endl;
//             free(compressed_data);
//             continue;
//         }

//         // a=T means "simultaneously transmit and display an image"
//         std::string cmd =
//             "a=T,o=z,f=32,s=" + std::to_string(width) + ",v=" + std::to_string(height);
//         kitty_send_command(cmd, compressed_data, compressed_size);
//         free(compressed_data);
//         std::cout << CSI << "H"; // move cursor to the top left

//         // Sleep for the frame delay
//         if (i < frames - 1) {         // Don't sleep after the last frame
//             int delay_ms = delays[i]; // Convert from 1/100th of a second to milliseconds
//             std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
//         }
//     }
//     // Clean up
//     stbi_image_free(data);
//     restore_terminal();
//     return 0;
// }
