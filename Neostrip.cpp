#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <spi/neostrip.h>
#include <sys/ioctl.h>
}

#include "Neostrip.h"
#include "utils.h"

#define PRINT_ERR(fmt, args...) \
    fprintf(stderr, "%s: " fmt, __func__, ##args)

Neostrip::Neostrip(size_t len) : fd(-1), len(len), scale(1.0)
{
    this->pixels = (uint32_t*)calloc(this->len, sizeof(*this->pixels));
    this->scaled_pixels = (uint32_t*)calloc(this->len, sizeof(*this->pixels));
    if (!this->pixels)
    {
        PRINT_ERR("calloc failed\n");
        this->len = 0;
    }
}

Neostrip::~Neostrip(void)
{
    this->close_fd();
    free(this->pixels);
    this->pixels = NULL;
}

int Neostrip::open_fd(void)
{
    this->fd = open(NEOSTRIP_DEV_FILE, O_RDONLY);
    if (this->fd < 0)
    {
        PRINT_ERR("failed to open %s\n", NEOSTRIP_DEV_FILE);
        return -1;
    }
    return 0;
}

void Neostrip::close_fd(void)
{
    if (this->fd > 0)
        close(this->fd);
    this->fd = -1;
}

int Neostrip::write(void)
{
    struct neostrip_ioc_data ioc_data = {
        .offset = 0,
        .count = this->len,
        .pixels = this->scaled_pixels,
    };
    return ioctl(this->fd, NEOSTRIP_IOC_WRITE, &ioc_data);
}

void Neostrip::set_scale(float scale)
{
    if (scale < 0.0)
        scale = 0.0;
    else if (scale > 1.0)
        scale = 1.0;

    this->scale = scale;
    for (size_t i = 0; i < this->len; i++)
        this->scaled_pixels[i] = scale_color(this->scale, this->pixels[i]);
}

void Neostrip::set_pixel(size_t n, uint32_t color)
{
    if (n >= this->len)
    {
        PRINT_ERR("invalid index: %u\n", n);
        return;
    }

    this->pixels[n] = color;
    this->scaled_pixels[n] = scale_color(this->scale, this->pixels[n]);
}

void Neostrip::set_pixel(size_t n, uint8_t red, uint8_t green, uint8_t blue)
{
    this->set_pixel(n, TO_RGB(red, green, blue));
}

void Neostrip::set_pixels(size_t offset, size_t count, const uint32_t *colors)
{
    for (size_t i = 0; i < count; i++)
        this->set_pixel(offset + i, colors[i]);
}

void Neostrip::set_pixels(size_t offset, size_t count, uint32_t color)
{
    for (size_t i = 0; i < count; i++)
        this->set_pixel(offset + i, color);
}

void Neostrip::set_all_pixels(uint32_t color)
{
    for (size_t i = 0; i < this->len; i++)
        this->set_pixel(i, color);
}

void Neostrip::set_all_pixels(uint8_t red, uint8_t green, uint8_t blue)
{
    this->set_all_pixels(TO_RGB(red, green, blue));
}

void Neostrip::clear(void)
{
    memset(this->pixels, 0, this->len * sizeof(*this->pixels));
    memset(this->scaled_pixels, 0, this->len * sizeof(*this->scaled_pixels));
}
