#ifndef _NEOSTRIP_API_H_
#define _NEOSTRIP_API_H_

#include <cstdint>

#define NEOSTRIP_DEV_FILE "/dev/neostrip0"

class Neostrip
{
    public:
        Neostrip(size_t len);
        ~Neostrip(void);

        int open_fd(void);
        void close_fd(void);
        int write(void);

        void set_scale(float scale);
        void set_pixel(size_t n, uint32_t color);
        void set_pixel(size_t n, uint8_t red, uint8_t green, uint8_t blue);
        void set_pixels(size_t offset, size_t count, const uint32_t *colors);
        void set_pixels(size_t offset, size_t count, uint32_t color);
        void set_all_pixels(uint32_t color);
        void set_all_pixels(uint8_t red, uint8_t green, uint8_t blue);
        void clear(void);

    protected:
        int         fd;
        size_t      len;
        float       scale;
        uint32_t    *pixels;
        uint32_t    *scaled_pixels;
};

#endif // _NEOSTRIP_API_H_
