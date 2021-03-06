# imp - an image processing utility
Graphics processing algorithms written in x86 assembly and C. Several versions are provided with an emphasis on optimizing for run-time speed. To that effect, SIMD and vectorization techniques will be heavily used and benchmarked.

## Screenshots
![Some results](/res/results.png)  
*Pictured from left to right: ordered dithering (8x8 Bayer matrix), 16 color palette quantization ([web-safe colors](https://www.w3.org/TR/REC-html40/types.html#h-6.5)), original*

## Algorithms Implemented
- Ordered dithering in C.
- Naive sum of absolute differences (SAD) in x86-64.
- Naive sum of absolute differences in C.

## Setup
```sh
git clone https://github.com/feydor/imp
cd imp
mkdir build && cd build
meson ..
ninja
./sadx64 -i [input_file] -o [output_file]
```

## Todo
- Add more options for SAD; ie greatest SAD.
- Complete the benchmarking function for each algorithm.
- SIMD optimizations on x86-64 and C versions.
- Win32 executable.

