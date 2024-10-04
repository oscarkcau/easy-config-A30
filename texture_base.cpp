#include "texture_base.h"

#include <iostream>
#include <SDL.h>
#include <SDL_image.h>

#include "global.h"

TextureBase::TextureBase(SDL_Surface *surface, TextureAlignment alignment)
{
    init(surface, alignment);
}

void TextureBase::init(SDL_Surface *surface, TextureAlignment alignment) {
    // init width and height
    w_ = surface->w;
    h_ = surface->h;

    // create texture
    createTexture(surface);

    // compute render rect
    updateTargetRect(alignment);

    // set flag
    isInitialized_ = true;
}

void TextureBase::createTexture(SDL_Surface *surface) {
   texture_ = SDLTextureUniquePtr {
        SDL_CreateTextureFromSurface(
            global::renderer,
            surface)
    };
    if (texture_ == nullptr)
        std::cerr << ("Texture creation failed") << std::endl;
}


void TextureBase::updateTargetRect(TextureAlignment alignment) {
    rect_ = {0, 0, 0, 0};
    rect_.w = w_;
    rect_.h = h_;
    switch (alignment) {
        case TextureAlignment::topCenter:
            rect_.x = -(w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2;
        break;
        case TextureAlignment::topLeft:
            rect_.x = -(w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2 + 
                (global::SCREEN_HEIGHT - w_) / 2;
        break;
        case TextureAlignment::topRight:
            rect_.x = -(w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2 - 
                (global::SCREEN_HEIGHT - w_) / 2;
        break;
        case TextureAlignment::bottomCenter:
            rect_.x = (global::SCREEN_WIDTH - w_) + (w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2;
        break;
        case TextureAlignment::bottomLeft:
            rect_.x = (global::SCREEN_WIDTH - w_) + (w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2 +
            	(global::SCREEN_HEIGHT - w_) / 2;
        break;
        case TextureAlignment::bottomRight:
            rect_.x = (global::SCREEN_WIDTH - w_) + (w_ - h_) / 2;
            rect_.y = (global::SCREEN_HEIGHT - h_) / 2 -
                (global::SCREEN_HEIGHT - w_) / 2;
        break;
    }
}


void TextureBase::render() const {
    SDL_RenderCopyEx(global::renderer, 
        texture_.get(), 
        nullptr, 
        &rect_,
        270, nullptr, SDL_FLIP_NONE
    );
}

void TextureBase::render(int offsetX, int offsetY) const {
    auto rect = rect_;
    rect.x += offsetY;
    rect.y -= offsetX;

    SDL_RenderCopyEx(global::renderer, 
        texture_.get(), 
        nullptr, 
        &rect,
        270, nullptr, SDL_FLIP_NONE
    );
}

void TextureBase::scrollLeft(int offset) {
    rect_.y += offset;
}

