#ifndef IMAGE_TEXTURE_H
#define IMAGE_TEXTURE_H

#include <string>

#include <SDL.h>

#include "texture_base.h"

class ImageTexture: public TextureBase
{
public:
    explicit ImageTexture(
        std::string filename, 
        TextureAlignment alignment = TextureAlignment::topLeft
    );
    virtual ~ImageTexture() = default;

    // disallow copying and assignment
    ImageTexture(const ImageTexture &) = delete;
    ImageTexture &operator=(const ImageTexture &) = delete;

    std::string getFilename() const { return filename_; }

private:

    std::string filename_;
};

#endif // IMAGE_TEXTURE_H