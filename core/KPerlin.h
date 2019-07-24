#pragma once

namespace mo {

float perlin1D(float lower, float upper, float x, int x_wrap);
float perlin2D(float lower, float upper, float x, float y, int x_wrap, int y_wrap);
float perlin3D(float lower, float upper, float x, float y, float z, int x_wrap, int y_wrap, int z_wrap);

}
