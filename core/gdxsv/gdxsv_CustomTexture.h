//
//  gdxsv_CustomTexture.h
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2021 flycast. All rights reserved.
//
#if defined(__APPLE__) || defined(_WIN32)
#pragma once

#include "../rend/CustomTexture.h"

class GDXCustomTexture : public CustomTexture {
public:
    bool Init() override;
#ifdef _WIN32
    u8* LoadCustomTexture(u32 hash, int& width, int& height) override;
protected:
    void LoadMap() override;
#endif
};
extern GDXCustomTexture gdx_custom_texture;
#endif
