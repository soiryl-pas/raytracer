# Raytracer

A raytracer that produces raytraced images with varying quality.
This raytracer was part of the Foundations of Graphics Computing course at the University of Vienna.

As a secondary goal, a lot of dedication went into exploring C++20 and C++23 features, even if they were absolutely unnecessary. (Looking at you, literal overloading)

## Build

On NixOS, just run:
```console
nix run .#fast -- --all ./res/provided
nix run .#fast -- --all ./res/custom
nix run .#fast -- ./res/custom/custom-one-render.xml
```

On other Linux distributions, you may want to install `zlib`, all other dependencies are already vendored.
The following commands may then be used for building and running the raytracer:
```console
cmake --preset fast
cmake --build ./build/fast

./build/fast/raytracer --all ./res/provided
./build/fast/raytracer --all ./res/custom
./build/fast/raytracer ./res/custom/custom-one-render.xml
```

While the provided scenes should be rendered very quickly, the two custom scenes may take a couple of minutes.
`res/generated` holds the already rendered images.

## Internals

The XML scene specifies a certain output image resolution.
For each pixel in this image, a ray is cast out of the virtual camera into the scene, where it intersects with an object of a textured or plainly coloured material.
The pixel associated to the ray is then coloured in accordance to the phong illumination colour, i.e. with ambient, diffuse and specular components, as long as there is nothing occluding the light source, which is checked with an additional shadow ray cast from the intersection point.
Point lights, directional lights, spot lights, and area lights have been implemented.
If the material is reflecting or transmitting, a reflected or refracting ray is additionally cast and contributes to the pixel's colour.
Furthermore, depth-of-field blur can be added to the image, which is however very noisy without rendering multiple times.
All of these properties can be toggled and specified in the XML scene description, see `res/**/*.xml` as reference.

Intersection tests are also accelerated by using a Bounding Volume Hierarchy, which has shown to be integral for the performance of the more complex custom scenes.
To turn this off and enjoy worse performance, pass `--nobvh` to the raytracer invocation.

