#include "text_texture.h"

#include <SDL.h>
#include <SDL_image.h>

TextTexture::TextTexture(std::string text, TTF_Font *font, SDL_Color color, 
    TextureAlignment alignment)
    : text_(text)
{
    // create surface
    SDL_Surface *surface = TTF_RenderUTF8_Blended(
        font,
        text.c_str(),
        color
    );
    
    init(surface, alignment);

    // free surface
	SDL_FreeSurface(surface);
} 

TextTexture::TextTexture(std::string text, TTF_Font *font, SDL_Color color, 
    TextureAlignment alignment, unsigned int wrapLength)
    : text_(text)
{
    // create surface
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(
        font,
        text.c_str(),
        color,
        wrapLength
    );

    init(surface, alignment);

    // free surface
	SDL_FreeSurface(surface);
} 

