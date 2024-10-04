#include "image_texture.h"

#include <iostream>
#include <SDL.h>
#include <SDL_image.h>

ImageTexture::ImageTexture(std::string filename, TextureAlignment alignment)
    : filename_(filename)
{
    // create surface
	SDL_Surface *surface = IMG_Load(filename.c_str());
    if (IMG_GetError() != nullptr && strcmp(IMG_GetError(), "") != 0) {
        std::cerr << "cannot load image: " << filename << std::endl;
        std::cerr << IMG_GetError() << std::endl;
        SDL_ClearError();
    }

    init(surface, alignment);

    // free surface
	SDL_FreeSurface(surface);
} 

